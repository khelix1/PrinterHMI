#!/usr/bin/env python3
"""Exercise the real Network page with the firmware demo backend."""
import importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup',Path(__file__).with_name('popup-parity-test.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoNetworkTests(m.PopupParityTests):
 def workflow(self,operator):
  if operator:self.operator()
  self.host.request('open network-page 0');self.assertIn('Offline demo',self.text());self.press('Scan networks')
  self.assertIn('Demo network',self.text());self.press('Demo network');self.assertIn('Wi-Fi password',self.text());self.press('Connect')
  self.assertIn('Demo network connected',self.text());self.assertIn('127.0.0.1',self.text());self.assertNotIn('Wi-Fi password',self.text())
 def test_studio(self):self.workflow(False)
 def test_operator(self):self.workflow(True)
if __name__=='__main__':sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoNetworkTests(n) for n in ('test_studio','test_operator'))).wasSuccessful())
