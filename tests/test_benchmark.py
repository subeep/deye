import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'src/detector/python'))
import benchmark as b

class FakeAnalyzer:
    calls = []
    def __init__(self, turbo): pass
    def analyze(self, samples, rate, frequency, timestamp, epoch):
        self.calls.append((len(samples),timestamp,epoch))
        return dict(events=[],candidates=0,rejected=0)

class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.m=b.load_manifest(ROOT/'tests/data/captures.json')
        t=tempfile.TemporaryDirectory(); self.addCleanup(t.cleanup)
        self.root=Path(t.name); FakeAnalyzer.calls=[]
    def validate(self,m):
        p=self.root/'m.json'; p.write_text(json.dumps(m)); return b.load_manifest(p)
    def test_session_leak_and_duplicate_ids(self):
        for key in ('id','session'):
            m=copy.deepcopy(self.m); m['captures'][1][key]=m['captures'][0][key]
            m['captures'][1]['split']='held_out'
            with self.assertRaises(ValueError): self.validate(m)
    def test_invalid_metadata(self):
        for key,val in [('sample_rate_hz',float('nan')),('format','int16'),('sha256','wrong')]:
            m=copy.deepcopy(self.m); m['captures'][0][key]=val
            with self.assertRaises(ValueError): self.validate(m)
    def test_hash_and_partial_sample(self):
        c=copy.deepcopy(self.m['captures'][0]); c['path']='bad.fc32'
        (self.root/c['path']).write_bytes(b'123')
        r=b.run_capture(c,self.root,'unused',20,FakeAnalyzer)
        self.assertEqual(r['status'],'error'); self.assertIn('SHA256',r['failures'][0])
        c['sha256']=hashlib.sha256(b'123').hexdigest()
        r=b.run_capture(c,self.root,'unused',20,FakeAnalyzer)
        self.assertIn('8-byte',r['failures'][0])
    def test_chunks_and_unknown_recovery(self):
        r=b.run_capture(self.m['captures'][2],self.root,'unused',7,FakeAnalyzer)
        self.assertEqual(r['status'],'pass')
        self.assertEqual([n for n,_,_ in FakeAnalyzer.calls],[14000,14000,12000])
        self.assertEqual([t for _,t,_ in FakeAnalyzer.calls],[0,.007,.014])
        self.assertIsNone(r['packet_recovery_fraction'])
    def test_missing_protocol_fails(self):
        r=b.run_capture(self.m['captures'][-1],self.root,'unused',20,FakeAnalyzer)
        self.assertEqual(r['status'],'fail')
        self.assertIn('Missing protocols',r['failures'][0])
    def test_deterministic_generator(self):
        c=self.m['captures'][2]
        self.assertEqual(b.generated(c).tobytes(),b.generated(c).tobytes())
if __name__=='__main__': unittest.main()
