#!/usr/bin/env python3
"""Run local OTA simulation using the firmware demo implementation."""
import importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup',Path(__file__).with_name('popup-parity-test.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoOtaTests(m.PopupParityTests):
 def start(self):
  self.ota();self.click(500,240);self.click(700,485);self.host.request('advance 300');self.assertIn('Simulating firmware transfer',self.text())
 def test_cancel(self):
  self.start();self.click(510,480);self.host.request('advance 300');self.assertIn('Simulation cancelled',self.text());self.host.request('key 27');self.assertNotIn('Simulation cancelled',self.text())
 def test_complete(self):
  self.start()
  for _ in range(12):self.host.request('advance 300')
  self.assertIn('Simulation complete',self.text());self.assertIn('No firmware was changed',self.text());self.press('Close');self.assertNotIn('Simulation complete',self.text())
if __name__=='__main__':sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoOtaTests(n) for n in ('test_cancel','test_complete'))).wasSuccessful())
