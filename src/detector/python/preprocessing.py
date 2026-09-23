"""Optional streaming DC blocker and integer-decimation channelizer.

Causal FIR state and mixer phase survive chunk boundaries; gaps reset both.
Output timestamps compensate FIR group delay. DC blocker phase is frequency
 dependent and is not timestamp-corrected. No gain normalization is performed.
"""
import math
import time
import numpy as np
from scipy import signal


class Preprocessor:
    def __init__(self, dc_block=False, shift_hz=0., output_rate_hz=None, bandwidth_hz=None):
        self.dc_block = dc_block
        self.shift = shift_hz
        self.requested_rate = output_rate_hz
        self.bandwidth = bandwidth_hz
        self.key = None
        self.expected = None
        self.generation = 0
        self.discarded = 0

    def configure(self, rate):
        values = [rate, self.shift] + [v for v in (self.requested_rate, self.bandwidth) if v is not None]
        if not all(math.isfinite(v) for v in values) or not 1e5 <= rate <= 100e6:
            raise ValueError('Invalid preprocessing rate/frequency')
        output = rate if self.requested_rate is None else self.requested_rate
        self.factor = round(rate/output) if output > 0 else 0
        if self.factor < 1 or not 1e5 <= output <= rate or not math.isclose(rate/self.factor, output, rel_tol=1e-9):
            raise ValueError('Output rate must divide input rate by a positive integer')
        self.rate = rate/self.factor
        self.taps = np.array([1.])
        if self.factor > 1 or self.bandwidth is not None or self.shift:
            bandwidth = self.bandwidth if self.bandwidth is not None else .8*self.rate
            if not 0 < bandwidth < self.rate:
                raise ValueError('Channel bandwidth must be positive and less than output rate')
            stop = min(self.rate/2, rate/2-abs(self.shift))
            edge = bandwidth/2
            if stop <= edge:
                raise ValueError('Selected channel/transition lies outside captured spectrum')
            width = (stop-edge)/(rate/2)
            length, beta = signal.kaiserord(60, width)
            length = max(3, length | 1)
            if length > 4095:
                raise ValueError('Transition too narrow; choose a wider guard band')
            self.taps = signal.firwin(length, (edge+stop)/2, fs=rate, window=('kaiser',beta))
        self.delay = (len(self.taps)-1)/2
        self.fir_state = np.zeros(len(self.taps)-1, np.complex128)
        self.dc_state = np.zeros(1, np.complex128)
        self.dc_initialized = False
        self.pending = np.empty(0, np.complex128)
        self.pole = math.exp(-2*math.pi*1000/rate)  # 1 kHz DC-blocker corner
        self.position = 0
        # Omit FIR startup; initialize DC state from a bounded first-block mean.
        self.warmup = len(self.taps)-1

    def process(self, samples, rate, frequency, timestamp, epoch):
        if not math.isfinite(timestamp) or not math.isfinite(frequency) or not np.isfinite(samples).all():
            raise ValueError('Invalid preprocessing samples/metadata')
        key = (rate, frequency, epoch)
        reset = key != self.key or self.expected is None or abs(timestamp-self.expected) > max(2/rate, 1e-9)
        if reset:
            self.configure(rate)
            self.key = key
            self.origin = timestamp
            self.generation += 1
        self.expected = timestamp + len(samples)/rate
        x = np.asarray(samples, dtype=np.complex128)
        if self.dc_block:
            gain = (1+self.pole)/2
            if not self.dc_initialized:
                x = np.concatenate((self.pending, x))
                if len(x) < 4096:
                    self.pending = x
                    return np.empty(0,np.complex64), self.rate, frequency+self.shift, self.origin, self.generation
                self.dc_state[0] = -gain*np.mean(x[:4096])
                self.pending = np.empty(0,np.complex128)
                self.dc_initialized = True
            x, self.dc_state = signal.lfilter([gain,-gain], [1,-self.pole], x, zi=self.dc_state)
        start = self.position
        if self.shift:
            phase = ((start*self.shift/rate) % 1) + np.arange(len(x))*(self.shift/rate)
            x = x*np.exp(-2j*np.pi*phase)
        if len(self.taps)>1:
            x, self.fir_state = signal.lfilter(self.taps, [1.], x, zi=self.fir_state)
        first = max(0, self.warmup-start)
        first += (-(start+first)) % self.factor
        self.position += len(x)
        self.discarded += min(len(x), max(0,self.warmup-start))
        out = np.asarray(x[first::self.factor],dtype=np.complex64)
        out_time = self.origin+(start+first-self.delay)/rate
        return out, self.rate, frequency+self.shift, out_time, self.generation


class PreparedAnalyzer:
    def __init__(self, analyzer, **config):
        self.analyzer = analyzer
        self.preprocessor = Preprocessor(**config)

    def analyze(self, samples, rate, frequency, timestamp=0., epoch=0):
        started = time.perf_counter()
        x, output_rate, center, output_time, generation = self.preprocessor.process(samples, rate, frequency, timestamp, epoch)
        preprocessing_ms = (time.perf_counter()-started)*1000
        if len(x):
            result = self.analyzer.analyze(x, output_rate, center, output_time, generation)
        else:
            result = dict(events=[], candidates=0, rejected=0, processing_ms=0)
        result['preprocessing_ms'] = preprocessing_ms
        result['processing_ms'] += preprocessing_ms
        result['preprocessing'] = dict(output_rate_hz=output_rate, center_frequency_hz=center,
            output_samples=len(x), filter_delay_input_samples=self.preprocessor.delay,
            warmup_discarded_input_samples=self.preprocessor.discarded)
        return result
