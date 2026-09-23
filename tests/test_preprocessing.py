import sys
from pathlib import Path
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src/detector/python'))
from preprocessing import Preprocessor

class PreprocessingTests(unittest.TestCase):
    def tone(self, frequency, rate=6000000, n=60000):
        return np.exp(2j*np.pi*frequency*np.arange(n)/rate).astype(np.complex64)
    def test_stream_matches_whole_with_mixing(self):
        x=self.tone(1200000)
        config=dict(dc_block=True,shift_hz=1000000,output_rate_hz=2000000)
        whole=Preprocessor(**config).process(x,6e6,2.4e9,5,1)
        p=Preprocessor(**config); ys=[]; times=[]; sizes=[]
        for start in range(0,len(x),713):
            out=p.process(x[start:start+713],6e6,2.4e9,5+start/6e6,1)
            if len(out[0]): ys.append(out[0]); times.append(out[3]); sizes.append(len(out[0]))
        np.testing.assert_allclose(np.concatenate(ys),whole[0],atol=1e-6)
        self.assertEqual(whole[1],2e6); self.assertEqual(whole[2],2.401e9)
        for i in range(1,len(times)):
            self.assertAlmostEqual(times[i],times[i-1]+sizes[i-1]/2e6,places=10)
        self.assertAlmostEqual(times[0],whole[3])
        phase=np.angle(np.vdot(whole[0][:-1],whole[0][1:]))
        self.assertAlmostEqual(phase*2e6/(2*np.pi),200000,delta=1)
    def test_antialias_and_amplitude(self):
        outputs=[]
        for frequency in (200000,1800000):
            p=Preprocessor(output_rate_hz=2e6)
            y=p.process(self.tone(frequency),6e6,2.4e9,0,0)[0]
            outputs.append(float(np.sqrt(np.mean(abs(y)**2))))
        self.assertAlmostEqual(outputs[0],1,delta=.005)
        self.assertLess(outputs[1],.001)
    def test_dc_rejection_and_tone_preservation(self):
        x=10+ self.tone(200000)
        y=Preprocessor(dc_block=True).process(x,6e6,2.4e9,0,0)[0]
        self.assertLess(abs(y.mean()),.005)
        self.assertAlmostEqual(float(np.std(y)),1,delta=.005)
    def test_gap_epoch_and_retune_reset(self):
        x=self.tone(200000,n=6000)
        for rate,freq,timestamp,epoch in ((6e6,2.4e9,1,1),(6e6,2.4e9,.001,2),(6e6,2.41e9,.001,1),(3e6,2.4e9,.001,1)):
            p=Preprocessor(dc_block=True)
            p.process(x,6e6,2.4e9,0,1)
            actual=p.process(x,rate,freq,timestamp,epoch)
            reference=Preprocessor(dc_block=True).process(x,rate,freq,timestamp,epoch)
            np.testing.assert_array_equal(actual[0],reference[0])
            self.assertEqual(actual[4],2)
    def test_invalid_configuration(self):
        for config in (dict(output_rate_hz=2.5e6),dict(output_rate_hz=0),dict(shift_hz=3e6,output_rate_hz=2e6),dict(bandwidth_hz=7e6)):
            with self.assertRaises(ValueError):
                Preprocessor(**config).process(self.tone(0),6e6,2.4e9,0,0)
    def test_passthrough(self):
        x=self.tone(200000)
        y=Preprocessor().process(x,6e6,2.4e9,10,1)
        np.testing.assert_array_equal(y[0],x);self.assertEqual(y[3],10)
if __name__=='__main__':unittest.main()
