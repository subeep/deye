#!/usr/bin/env python3
"""Receive-only IQ analysis. Same engine serves the UI and offline capture tests."""
import argparse
import contextlib
import ctypes
import json
import os
from pathlib import Path
import struct
import sys
import time
import warnings

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/drone-detector-mpl")
sys.path.insert(0, str(Path(__file__).parent / "reference"))
import numpy as np
from scipy import signal
from SpectrumCapture import SpectrumCapture
from Packet import Packet
from qpsk import Decoder
from goldgen import gold
from droneid_packet import DroneIDPacket
from packetizer import coarse_activity

HEADER = struct.Struct("<4sIdddQ")
MAX_SAMPLES = 2_000_000


def crc24(data):
    crc = 0
    for byte in data:
        crc ^= int(byte) << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= 0x1864CFB
    return crc & 0xffffff


class Turbo:
    def __init__(self, path):
        self.lib = ctypes.CDLL(str(path))
        self.lib.drone_turbo_decode.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.lib.drone_turbo_decode.restype = ctypes.c_int
        self.scrambler = gold(1600, 7200, 0x12345678).astype(bool)

    def decode(self, symbols):
        values = np.delete(np.asarray(symbols, dtype=np.uint8), 300, axis=1)
        bits = ((np.repeat(values, 2, axis=1) & np.tile([1, 2], 600)) != 0)
        bits = bits[-6:].reshape(-1)
        if len(bits) != 7200:
            raise ValueError("Invalid DJI symbol count")
        soft = np.where(bits ^ self.scrambler, 63, -63).astype(np.int8)
        output = np.zeros(176, dtype=np.uint8)
        if self.lib.drone_turbo_decode(soft.ctypes.data, output.ctypes.data):
            return None
        raw = output.tobytes()
        return raw if crc24(raw) == 0 else None


def observation(protocol, link, evidence, frequency, power, confirmed=False, **fields):
    return dict(protocol=protocol, link=link, evidence=evidence, frequency=frequency,
                power=power, confirmed=confirmed, **fields)


def css_signature(iq, rate):
    """Repeated LoRa chirps: require 5 stable dechirped peaks, reject a CW tone.

    Detects modulation only, not an ExpressLRS packet or aircraft identity.
    Searches both chirp polarities and SX127x/SX128x bandwidths.
    """
    for bandwidth in (500_000, 812_500, 1_625_000):
        if rate < bandwidth * 1.05:
            continue
        # Bounded rational polyphase resampling, then one sample per chirp chip.
        from fractions import Fraction
        ratio = Fraction(bandwidth / rate).limit_denominator(1024)
        y = signal.resample_poly(iq, ratio.numerator, ratio.denominator)
        for sf in range(5, 10):
            n = 1 << sf
            if len(y) < 7 * n:
                continue
            t = np.arange(n)
            chirp = np.exp(1j * np.pi * (t * t / n - t))
            for direction in (1, -1):
                reference = chirp if direction == 1 else chirp.conj()
                blocks = y[:min(len(y) // n, 256) * n].reshape(-1, n)
                spectra = np.abs(np.fft.fft(blocks * reference.conj(), axis=1)) ** 2
                peaks = spectra.argmax(axis=1)
                concentration = spectra.max(axis=1) / (spectra.sum(axis=1) + 1e-30)
                good = concentration > .32
                run = 0
                for i in range(1, len(peaks)):
                    delta = abs(int(peaks[i]) - int(peaks[i-1]))
                    run = run + 1 if good[i] and good[i-1] and min(delta, n-delta) <= 1 else 0
                    if run >= 4:
                        return sf, bandwidth, float(np.median(concentration[good]))
    return None


def cp_score(iq, rate, useful_us, prefix_us):
    lag, width = int(round(rate * useful_us * 1e-6)), int(round(rate * prefix_us * 1e-6))
    if width < 16 or len(iq) < 6 * (lag + width):
        return 0.0
    a, b = iq[:-lag], iq[lag:]
    def rolling(x):
        sums = np.concatenate(([0], np.cumsum(x)))
        return sums[width:] - sums[:-width]
    cross = np.abs(rolling(a * b.conj()))
    norm = np.sqrt(rolling(np.abs(a)**2).clip(0) * rolling(np.abs(b)**2).clip(0))
    metric = cross / (norm + 1e-20)
    peaks, _ = signal.find_peaks(metric, height=.65, prominence=.3, distance=max(1, int(.85*(lag+width))))
    if len(peaks) < 5:
        return 0.0
    intervals = np.diff(peaks)
    regular = np.abs(intervals - (lag + width)) < max(3, .08*(lag+width))
    # Require a consecutive symbol train; scattered matches in noise are insufficient.
    run = 0
    for match in regular:
        run = run + 1 if match else 0
        if run >= 4:
            return float(np.median(metric[peaks]))
    return 0.0


def classify_waveform(iq, rate, frequency):
    power = float(10*np.log10(np.mean(np.abs(iq)**2) + 1e-20))
    if power < -100 or len(iq) < 4096:
        return []
    _, psd = signal.welch(iq[:32768], rate, nperseg=1024, return_onesided=False)
    psd = np.fft.fftshift(psd)
    floor = float(np.percentile(psd, 25)) + 1e-30
    active = psd > max(floor * 10, float(psd.max()) * .001)
    # Broad noise without structured evidence does not become a drone detection.
    css = css_signature(iq, rate) if rate <= 5e6 else None
    if css:
        sf, bw, score = css
        return [observation("LoRa CSS / ELRS candidate", "Control / telemetry candidate",
                f"Repeated dechirped preamble, SF{sf}, BW {bw/1e3:g} kHz; packet CRC/UID not decoded",
                frequency, power, bandwidth=bw, score=score)]
    for name, useful, prefix in (("Wi-Fi-like OFDM", 3.2, .8), ("DJI-like OFDM", 66.6667, 4.6875)):
        if rate < (20e6 if useful < 10 else 15e6):
            continue
        score = cp_score(iq[:int(rate*.002)], rate, useful, prefix)
        if score > .65:
            return [observation(name, "Digital link; role unknown",
                    f"Repeated cyclic prefixes ({useful:g} us useful symbol); no validated packet",
                    frequency, power, score=score)]
    if np.count_nonzero(active) >= 3:
        bins = np.where(active)[0]
        bandwidth = float((bins[-1] - bins[0] + 1) * rate / len(psd))
        # Two populated clusters in phase increments are consistent with 2-FSK.
        phase = np.angle(iq[1:] * iq[:-1].conj()) * rate / (2*np.pi)
        strong = np.abs(iq[1:]) > np.median(np.abs(iq)) * .5
        phase = phase[strong]
        if len(phase) > 100 and bandwidth < 2e6:
            centers = np.percentile(phase, [25, 75])
            for _ in range(8):
                mask = abs(phase-centers[0]) < abs(phase-centers[1])
                if not mask.any() or mask.all():
                    break
                centers = np.array([phase[mask].mean(), phase[~mask].mean()])
            residual = np.minimum(abs(phase-centers[0]), abs(phase-centers[1]))
            separation = abs(centers[1]-centers[0])
            if separation > max(1000, 6*np.std(residual)) and .15 < mask.mean() < .85:
                return [observation("2-FSK candidate", "Control / telemetry candidate",
                        "Two frequency states; FrSky/FlySky/other FSK cannot be separated without packets",
                        frequency, power, bandwidth=bandwidth)]
        if bandwidth > 2e6 and 5.6e9 < frequency < 6e9:
            # Analog FM video has a horizontal-sync repetition after FM demodulation.
            phase = np.angle(iq[1:] * iq[:-1].conj())
            decimation = max(1, int(rate / 100000))
            video = signal.resample_poly(phase, 1, decimation)
            vrate = rate/decimation
            vf, vp = signal.welch(video, vrate, nperseg=min(4096, len(video)))
            band = (vf > 15000) & (vf < 16500)
            if band.any() and vp[band].max() > 20*np.median(vp + 1e-30):
                peak = float(vf[band][vp[band].argmax()])
                if min(abs(peak-15625), abs(peak-15734)) < 120:
                    return [observation("Analog FM video candidate", "Video downlink candidate",
                            f"FM-demodulated line-sync peak {peak:.0f} Hz; aircraft identity unknown",
                            frequency, power, bandwidth=bandwidth)]
        return [observation("Unknown RF", "Unclassified",
                            "Occupied spectrum; insufficient protocol evidence", frequency, power, bandwidth=bandwidth)]
    return []


class Analyzer:
    def __init__(self, turbo_path):
        self.turbo = Turbo(turbo_path)
        self.tail = np.empty(0, np.complex64)
        self.key = None
        self.seen = {}

    def analyze(self, samples, rate, frequency, timestamp=0., epoch=0):
        if not np.isfinite(rate) or not 1e5 <= rate <= 100e6 or not np.isfinite(frequency):
            raise ValueError("Invalid capture metadata")
        if len(samples) > MAX_SAMPLES or not np.isfinite(samples).all():
            raise ValueError("Invalid IQ samples")
        key = (rate, frequency, epoch)
        if key != self.key:
            self.tail = np.empty(0, np.complex64)
            self.seen.clear()
            self.key = key
        raw = np.concatenate((self.tail, samples))
        self.tail = raw[-int(rate*.002):].copy()
        started = time.monotonic()
        events, candidates, rejected = [], 0, 0
        if len(raw) < 4096:
            return dict(events=[], candidates=0, rejected=0, processing_ms=0)
        # Do not run the DJI demodulator on a band-limited DIY acquisition.
        if rate >= 15.26e6 and (2.3e9 < frequency < 2.6e9 or 5.6e9 < frequency < 6e9):
            with open(os.devnull, "w") as sink, contextlib.redirect_stdout(sink), warnings.catch_warnings():
                warnings.simplefilter("ignore")
                activity = coarse_activity(raw, rate)
                for legacy in (False, True):
                    capture = SpectrumCapture(raw, Fs=rate, legacy=legacy, activity=activity)
                    candidates += len(capture.packets)
                    for i in range(min(len(capture.packets), 24)):
                        try:
                            frame = capture.get_packet_samples(i)
                            packet = Packet(frame, enable_zc_detection=False, legacy=legacy)
                            decoder = Decoder(packet.get_symbol_data(skip_zc=True))
                            payload = None
                            for rotation in range(4):
                                decoder.raw_data_to_symbol_bits(rotation)
                                decoded = self.turbo.decode(decoder.sym_bits)
                                if decoded is None:
                                    continue
                                parsed = DroneIDPacket(decoded)
                                if parsed.check_crc() and parsed.droneid["pkt_len"] == 88 and parsed.droneid["version"] in (1, 2):
                                    payload = dict(parsed.droneid)
                                    break
                            if payload is None:
                                rejected += 1
                                continue
                            serial = payload["serial_number"]
                            if not serial or not serial.isprintable():
                                rejected += 1
                                continue
                            identity = (serial, payload["sequence_number"], payload["gps_time"])
                            if identity in self.seen:
                                continue
                            self.seen[identity] = timestamp
                            if len(self.seen) > 512:
                                del self.seen[next(iter(self.seen))]
                            power = float(10*np.log10(np.mean(np.abs(capture.packets[i])**2)+1e-20))
                            events.append(observation("DJI DroneID", "Identification broadcast",
                                "OFDM + descrambling + turbo FEC + CRC24A + payload CRC16 valid",
                                frequency, power, True, serial=serial,
                                model=payload["device_type"] or "Unknown DJI model",
                                sequence=payload["sequence_number"], latitude=payload["latitude"],
                                longitude=payload["longitude"], altitude=payload["altitude"],
                                legacy=legacy))
                        except (ValueError, IndexError, TypeError, UnicodeError, struct.error, FloatingPointError):
                            rejected += 1
        if not events:
            events = classify_waveform(samples, rate, frequency)
        for event in events:
            event["timestamp"] = timestamp
        return dict(events=events, candidates=candidates, rejected=rejected,
                    processing_ms=(time.monotonic()-started)*1000)


def read_exact(stream, size):
    buf = bytearray()
    while len(buf) < size:
        part = stream.read(size-len(buf))
        if not part:
            if not buf:
                return None
            raise EOFError("Truncated IQ message")
        buf.extend(part)
    return bytes(buf)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--turbo", required=True)
    parser.add_argument("--input")
    parser.add_argument("--rate", type=float, default=50e6)
    parser.add_argument("--frequency", type=float, default=2444.5e6)
    parser.add_argument("--dc-block", action="store_true")
    args = parser.parse_args()
    analyzer = Analyzer(args.turbo)
    if args.dc_block:
        from preprocessing import PreparedAnalyzer
        analyzer = PreparedAnalyzer(analyzer, dc_block=True)
    if args.input:
        with open(args.input, "rb") as stream:
            position = 0
            while True:
                data = stream.read(min(MAX_SAMPLES, int(args.rate*.02))*8)
                if not data:
                    break
                samples = np.frombuffer(data, "<c8")
                print(json.dumps(analyzer.analyze(samples, args.rate, args.frequency, position/args.rate), allow_nan=False), flush=True)
                position += len(samples)
        return
    print(json.dumps({"ready": True, "fec": "LTE turbo + CRC24A + CRC16"}), flush=True)
    while True:
        header = read_exact(sys.stdin.buffer, HEADER.size)
        if header is None:
            break
        magic, count, rate, frequency, timestamp, epoch = HEADER.unpack(header)
        if magic != b"DDIQ" or count > MAX_SAMPLES:
            raise ValueError("Invalid IQ message")
        payload = read_exact(sys.stdin.buffer, count*8)
        if payload is None:
            raise EOFError("Missing IQ payload")
        try:
            result = analyzer.analyze(np.frombuffer(payload, "<c8"), rate, frequency, timestamp, epoch)
        except Exception as exc:
            result = {"error": f"{type(exc).__name__}: {exc}", "events": []}
        print(json.dumps(result, allow_nan=False), flush=True)


if __name__ == "__main__":
    main()
