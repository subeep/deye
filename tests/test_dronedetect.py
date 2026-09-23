import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src/detector/python'))
import benchmark as b
import dronedetect as d

class FakeAnalyzer:
    calls=[]
    def __init__(self,turbo): pass
    def analyze(self,samples,rate,freq,timestamp,epoch):
        self.calls.append((samples.copy(),timestamp))
        return dict(events=[],candidates=0,rejected=0)

class DatasetTests(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory(); self.addCleanup(t.cleanup)
        self.root=Path(t.name); FakeAnalyzer.calls=[]
    def prepare(self):
        p=self.root/'dataset/CLEAN/MP1_ON/MA1_0000_00.dat'
        p.parent.mkdir(parents=True)
        np.arange(200000,dtype=np.float32).astype('<c8').tofile(p)
        audit,c=d.prepare(self.root/'dataset',self.root/'audit',100000,2437500000,20)
        return p,audit,c
    def test_labels(self):
        self.assertEqual(d.labels('WIFI/MP2_FY/MAV_1010_04.dat')['repeat'],4)
        for path in ('CLEAN/MP1_ON/MP1_0000_00.dat','WIFI/MP2_ON/MAV_0000_00.dat','CLEAN/AIR_ON/AIR_0000_05.dat'):
            with self.assertRaises(ValueError): d.labels(path)
    def test_exact_window_and_timestamp(self):
        p,audit,c=self.prepare()
        self.assertEqual(len(c),3)
        self.assertEqual(audit['recordings'],1)
        self.assertEqual(audit['errors'],[])
        r=b.run_capture(c[0],self.root,'unused',20,FakeAnalyzer)
        self.assertEqual(r['status'],'observed')
        x,t=FakeAnalyzer.calls[0]
        self.assertEqual(t,.25)
        np.testing.assert_array_equal(x,np.arange(25000,27000))
        self.assertEqual(r['input_sha256'],hashlib.sha256(x.astype('<c8').tobytes()).hexdigest())
        with p.open('r+b') as f: f.seek(25000*8); f.write(b'abcdefgh')
        r=b.run_capture(c[0],self.root,'unused',20,FakeAnalyzer)
        self.assertEqual(r['status'],'error')
        self.assertIn('SHA256',r['failures'][0])
    def test_bounds_and_recording_split(self):
        p,audit,c=self.prepare()
        manifest=dict(schema_version=1,captures=c)
        def validate():
            path=self.root/'m.json'; path.write_text(json.dumps(manifest)); b.load_manifest(path)
        validate()
        c[1]['split']='held_out'; c[1]['session']='different-name'
        with self.assertRaisesRegex(ValueError,'recording'): validate()
        c[1]['split']='development'
        c[0]['sample_count']=100000000
        with self.assertRaisesRegex(ValueError,'exceeds'): validate()
    def test_changed_source_and_truncation(self):
        p,audit,c=self.prepare()
        with p.open('ab') as f:f.write(b'12345678')
        self.assertIn('size changed',b.run_capture(c[0],self.root,'unused',20,FakeAnalyzer)['failures'][0])
        with self.assertRaises(ValueError): b.window_digest(p,200000,2)
    def test_probe_dc_and_nonfinite(self):
        p=self.root/'probe.dat'; np.ones(64,dtype='<c8').tofile(p)
        self.assertEqual(d.probe(p)[0]['dc_power_fraction'],1)
        np.array([complex(float('nan'),0)],dtype='<c8').tofile(p)
        self.assertEqual(d.probe(p)[0]['finite_fraction'],0)
if __name__=='__main__':unittest.main()
