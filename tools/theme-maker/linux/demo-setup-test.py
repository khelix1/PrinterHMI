#!/usr/bin/env python3
"""Exercise the tablet demo Setup Center's local Wi-Fi workflow."""
import importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup',Path(__file__).with_name('popup-parity-test.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoSetupTests(m.PopupParityTests):
 def workflow(self,operator):
  if operator:self.operator()
  self.host.request('open setup 0');self.press('Continue');self.press('Find a network');self.host.request('advance 500')
  self.assertIn('Demo network',self.text());self.press('Demo network  |  -48 dBm');self.press('Connect')
  self.host.request('advance 1000');self.assertIn('Wi-Fi verified',self.text())
  self.host.request('advance 1000');self.assertIn('Demo network',self.text())
  self.press('Set up later')
 def test_studio(self):self.workflow(False)
 def test_operator(self):self.workflow(True)
if __name__=='__main__':sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoSetupTests(n) for n in ('test_studio','test_operator'))).wasSuccessful())
