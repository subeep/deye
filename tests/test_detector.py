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
    def test_telemetry_optional_fields_and_flags(self):
        import json
        from telemetry import packet_telemetry, position_status
        self.assertEqual(position_status(None, 0), "Unavailable")
        self.assertEqual(position_status(0, 0), "Unavailable (zero pair)")
        self.assertEqual(position_status(91, 0), "Invalid coordinate range")
        self.assertEqual(position_status(float('nan'), 1), "Invalid coordinate range")
        self.assertEqual(position_status(0, 10), "Reported; validity unverified")
        self.assertIn("unset", position_status(10, 0, False))
        payload = dict(latitude=12., longitude=34., state_info=0xFFFF,
                       v_north=-12, v_east=0, v_up=45, gps_time=18446744073709551615,
                       sequence_number=65535, uuid="test", uuid_hex="74657374", uuid_len=4,
                       uuid_length_valid=True)
        t = packet_telemetry(payload)
        self.assertEqual(json.loads(t['packet_json']), payload)
        fields = {f['name']:f for f in t['fields']}
        self.assertEqual(fields['v_north (raw)']['value'], '-12')
        self.assertEqual(fields['v_east (raw)']['value'], '0')
        self.assertEqual(fields['Uninterpreted state bits']['value'], '0x00F9')
        self.assertEqual(fields['Bit 9: Private mode disabled']['value'], 'Set')
        self.assertIn('unverified', fields['gps_time']['status'])
        # Missing fields never acquire fabricated zero values or inherit prior values.
        empty = {f['name']:f for f in packet_telemetry({})['fields']}
        self.assertEqual(empty['v_north (raw)']['value'], 'Unavailable')
        self.assertEqual(empty['Bit 14: GPS valid']['value'], 'Unavailable')
        payload['state_info'] = 0
        self.assertIn('unset', packet_telemetry(payload)['aircraft_position_status'])

    def test_uuid_bytes_length_and_raw_payload_crc(self):
        import struct
        import crcmod
        from droneid_packet import DroneIDPacket, CRC_POLY, CRC_INIT
        from telemetry import packet_telemetry
        for length, content in ((0, b'ignored'), (3, b'abcTRAILING'), (2, b'\xff\xfe'), (25, b'x'*20)):
            raw = struct.pack('<BBBHH16siihhhhhhQiiiiBB20sH',
                              88, 0, 2, 65535, 0, b'TEST-SERIAL',
                              0, 0, -5, 10, -1, 0, 2, 3, 0,
                              0, 0, 0, 0, 63, length, content, 0)
            crc = crcmod.mkCrcFun(CRC_POLY, initCrc=CRC_INIT, rev=True)(raw[:-2])
            raw = raw[:-2] + struct.pack('<H', crc)
            packet = DroneIDPacket(raw)
            self.assertTrue(packet.check_crc())
            p = packet.droneid
            self.assertEqual(p['raw_payload_hex'], raw.hex())
            self.assertEqual(p['altitude_raw'], -5)
            self.assertEqual(p['uuid_length_valid'], length <= 20)
            if length == 0: self.assertEqual(p['uuid'], '')
            if length == 3: self.assertEqual(p['uuid'], 'abc')
            if length == 2: self.assertEqual(p['uuid_hex'], 'fffe')
            if length == 25:
                fields = {f['name']:f for f in packet_telemetry(p)['fields']}
                self.assertIn('Invalid length', fields['UUID']['status'])

    def analyzer(self):
        return backend.Analyzer(TURBO)

    def capture(self, name):
        return np.fromfile(ROOT / "tests/data" / name, "<c8")

    def test_adjacent_carrier_correction_both_directions(self):
        a = self.analyzer()
        with contextlib.redirect_stdout(io.StringIO()):
            capture = backend.SpectrumCapture(self.capture("dji_mini2.fc32"), Fs=50e6)
            frame = capture.get_packet_samples(0)
            for injected in (-15000, 15000):
                shifted = frame*np.exp(2j*np.pi*injected*np.arange(len(frame))/15.36e6)
                payload, correction = a.decode_frame(shifted)
                self.assertIsNotNone(payload)
                self.assertEqual(payload['sequence_number'],786)
                self.assertEqual(correction,-injected)

    def test_full_timing_fallback(self):
        from unittest.mock import patch
        packet_type=backend.Packet
        a=self.analyzer()
        with contextlib.redirect_stdout(io.StringIO()):
            capture=backend.SpectrumCapture(self.capture("dji_mini2.fc32"),Fs=50e6)
            frame=capture.get_packet_samples(0)
            def reject_fast(*args,**kwargs):
                if kwargs.get('fast_timing'): raise ValueError('forced fast-search miss')
                return packet_type(*args,**kwargs)
            with patch.object(backend,'Packet',side_effect=reject_fast):
                payload,correction=a.decode_frame(frame)
                self.assertEqual(payload['sequence_number'],786)

    def test_20msps_known_captures(self):
        from preprocessing import Preprocessor
        for name,shift,count in (("dji_mini2.fc32",9.6e6,8),("dji_mavic_air_2.fc32",-12.5e6,1)):
            raw=self.capture(name)
            x=Preprocessor(shift_hz=shift,output_rate_hz=25e6).process(raw,50e6,2444.5e6,0,0)[0]
            x=signal.resample_poly(x,4,5).astype(np.complex64)
            result=self.analyzer().analyze(x,20e6,2444.5e6+shift)
            self.assertEqual(sum(e['confirmed'] for e in result['events']),count)

    def test_channelized_mini2_preserves_all_ids(self):
        from preprocessing import PreparedAnalyzer
        a = PreparedAnalyzer(self.analyzer(), dc_block=True, shift_hz=9.6e6, output_rate_hz=25e6)
        result = a.analyze(self.capture("dji_mini2.fc32"), 50e6, 2444.5e6)
        valid = [e for e in result['events'] if e['confirmed']]
        self.assertEqual(len(valid), 8)
        self.assertEqual({e['sequence'] for e in valid}, {786,787,788,789,800,804,805,806})
        self.assertTrue(any(e['carrier_correction_hz']==15000 for e in valid))

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
        import json
        payload = json.loads(confirmed[0]['telemetry']['packet_json'])
        self.assertEqual(payload['sequence_number'], 591)
        self.assertEqual(len(bytes.fromhex(payload['raw_payload_hex'])), 91)
        self.assertEqual(payload['latitude'], confirmed[0]['latitude'])
        self.assertIn('v_north', payload)
        self.assertIn('state_info', payload)

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
