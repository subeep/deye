import contextlib
import io
from pathlib import Path
import sys
import unittest
import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src/detector/python"))
import backend

TURBO = sys.argv.pop(1)


class DetectorTests(unittest.TestCase):
    def analyzer(self):
        return backend.Analyzer(TURBO)

    def capture(self, name):
        return np.fromfile(ROOT / "tests/data" / name, "<c8")

    def test_channelized_mavic_air(self):
        from preprocessing import PreparedAnalyzer
        a = PreparedAnalyzer(self.analyzer(), dc_block=True, shift_hz=-12.5e6, output_rate_hz=25e6)
        result = a.analyze(self.capture("dji_mavic_air_2.fc32"), 50e6, 2444.5e6)
        valid = [e for e in result['events'] if e['confirmed']]
        self.assertEqual(len(valid), 1)
        self.assertEqual(valid[0]['model'], 'Mavic Air 2')
        self.assertEqual(valid[0]['sequence'], 591)

    def test_mavic_air_real_capture(self):
        result = self.analyzer().analyze(self.capture("dji_mavic_air_2.fc32"), 50e6, 2444.5e6)
        confirmed = [e for e in result["events"] if e["confirmed"]]
        self.assertEqual(len(confirmed), 1)
        self.assertEqual(confirmed[0]["model"], "Mavic Air 2")
        self.assertEqual(confirmed[0]["sequence"], 591)
        self.assertAlmostEqual(confirmed[0]["latitude"], 51.44633393111904)

    def test_mini2_real_capture_and_deduplication(self):
        a = self.analyzer()
        raw = self.capture("dji_mini2.fc32")
        events = a.analyze(raw, 50e6, 2444.5e6)["events"]
        valid = [e for e in events if e["confirmed"]]
        self.assertGreaterEqual(len(valid), 7)
        self.assertTrue(all(e["model"] == "Mini 2" for e in valid))
        again = a.analyze(raw, 50e6, 2444.5e6)["events"]
        self.assertFalse(any(e["confirmed"] for e in again))

    def test_noise_and_cw_are_not_protocols(self):
        rng = np.random.default_rng(2026)
        for rate in (2e6, 4e6, 25e6):
            n = int(rate*.02)
            signals = [(rng.normal(size=n)+1j*rng.normal(size=n))*.01,
                       .1*np.exp(2j*np.pi*100e3*np.arange(n)/rate), np.zeros(n)]
            for raw in signals:
                events = backend.classify_waveform(raw.astype(np.complex64), rate, 2444.5e6)
                self.assertTrue(all(e["protocol"] == "Unknown RF" for e in events), events)

    def test_css_preamble_and_not_elrs_confirmation(self):
        n = 128
        t = np.arange(n)
        chirp = np.exp(1j*np.pi*(t*t/n-t))
        raw = signal.resample_poly(np.tile(chirp, 12), 4, 1).astype(np.complex64)
        events = backend.classify_waveform(raw, 2e6, 915e6)
        self.assertEqual(events[0]["protocol"], "LoRa CSS / ELRS candidate")
        self.assertFalse(events[0]["confirmed"])
        # One chirp is insufficient protocol evidence.
        self.assertIsNone(backend.css_signature(raw[:512], 2e6))

    def test_fsk_candidate(self):
        rng = np.random.default_rng(1)
        bits = np.repeat(rng.integers(0,2,1000)*2-1, 40)
        raw = (.1*np.exp(2j*np.pi*np.cumsum(bits)*100e3/2e6)).astype(np.complex64)
        events = backend.classify_waveform(raw, 2e6, 2440e6)
        self.assertEqual(events[0]["protocol"], "2-FSK candidate")
        self.assertFalse(events[0]["confirmed"])

    def test_wifi_ofdm_candidate(self):
        rng = np.random.default_rng(3)
        symbols = np.fft.ifft(np.exp(1j*np.pi/2*rng.integers(0,4,(200,64))))
        raw = np.concatenate((symbols[:,-16:],symbols),axis=1).reshape(-1).astype(np.complex64)
        events = backend.classify_waveform(raw, 20e6, 2440e6)
        self.assertEqual(events[0]["protocol"], "Wi-Fi-like OFDM")
        self.assertFalse(events[0]["confirmed"])

    def test_analog_video_candidate(self):
        rate = 25e6
        t = np.arange(500000)/rate
        video = .3*np.sin(2*np.pi*15625*t) + .12*np.sin(2*np.pi*2e6*t)
        raw = (.1*np.exp(2j*np.pi*np.cumsum(video)*4e6/rate)).astype(np.complex64)
        events = backend.classify_waveform(raw,rate,5800e6)
        self.assertEqual(events[0]["protocol"], "Analog FM video candidate")
        self.assertFalse(events[0]["confirmed"])

    def test_stream_boundary_and_gap(self):
        raw = self.capture("dji_mavic_air_2.fc32")
        with contextlib.redirect_stdout(io.StringIO()):
            capture = backend.SpectrumCapture(raw, Fs=50e6)
        frame = capture.packets[-1].astype(np.complex64)
        midpoint = len(frame)//2
        prefix = np.concatenate((np.zeros(5000,np.complex64),frame[:midpoint]))
        suffix = np.concatenate((frame[midpoint:],np.zeros(5000,np.complex64)))
        a = self.analyzer()
        a.analyze(prefix,50e6,2444.5e6,0,1)
        self.assertTrue(any(e["confirmed"] for e in a.analyze(suffix,50e6,2444.5e6,.001,1)["events"]))
        a = self.analyzer()
        a.analyze(prefix,50e6,2444.5e6,0,1)
        self.assertFalse(any(e["confirmed"] for e in a.analyze(suffix,50e6,2444.5e6,.001,2)["events"]))

    def test_invalid_metadata_and_iq(self):
        a = self.analyzer()
        for rate in (0, float("nan"), 1e12):
            with self.assertRaises(ValueError):
                a.analyze(np.zeros(4096,np.complex64),rate,2444e6)
        with self.assertRaises(ValueError):
            a.analyze(np.array([complex(float("nan"),0)]),2e6,915e6)
        with self.assertRaises(EOFError):
            backend.read_exact(io.BytesIO(b"abc"),4)

    def test_crc24_check_vector(self):
        self.assertEqual(backend.crc24(b"123456789"),0xCDE703)


if __name__ == "__main__":
    unittest.main()
