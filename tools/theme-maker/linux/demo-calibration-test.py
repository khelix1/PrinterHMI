#!/usr/bin/env python3
"""Exercise real Calibration widgets against the tablet-demo backend."""
import importlib.util, sys, unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoCalibrationTests(m.PopupParityTests):
 def workflow(self, operator):
  if operator: self.operator()
  self.host.request('scenario idle');self.host.request('open calibration-page 0');self.host.request('advance 600')
  self.assertIn('Calibration',self.text())
  self.press('Accuracy');self.assertIn('Check probe accuracy?',self.text())
  self.assertEqual(self.press('Run check')['command'],'PROBE_ACCURACY SAMPLES=10')
  self.host.request('advance 600');self.assertIn('probe accuracy results',self.text());self.press('Close')
  self.press('Shaper');self.assertIn('Run input shaper calibration?',self.text())
  self.assertEqual(self.press('Run')['command'],'SHAPER_CALIBRATE');self.host.request('advance 600')
  self.assertIn('shaper_type_x=mzv',self.text());self.press('Apply & restart')
  self.assertIn('No printer will be changed or restarted',self.text())
  self.assertEqual(self.press('Apply & restart')['command'],'SAVE_CONFIG')
  self.assertIn('Demo: simulated save',self.text());self.press('Close')
  self.host.request('scenario printing');self.host.request('open calibration-page 0');self.host.request('advance 600')
  self.press('Accuracy');self.assertNotIn('Check probe accuracy?',self.text())
 def test_studio(self):self.workflow(False)
 def test_operator(self):self.workflow(True)
if __name__=='__main__':
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoCalibrationTests(n) for n in ('test_studio','test_operator'))).wasSuccessful())
