#!/usr/bin/env python3
"""Exercise new demo services through the real widgets in both themes."""
import importlib.util, sys, unittest
from pathlib import Path
spec = importlib.util.spec_from_file_location('popup_parity', Path(__file__).with_name('popup-parity-test.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class DemoServicesTests(m.PopupParityTests):
 def services(self, operator):
  if operator: self.operator()
  self.host.request('scenario idle')
  self.host.request('open devices-page 0')
  self.assertIn('Devices', self.text())
  self.assertIn('24', self.text())
  self.click(900, 104)
  self.assertIn('Endstop', self.text())
  self.host.request('key 27')
  self.host.request('open macros-page 0')
  self.assertIn('public macros', self.text())
  self.press('CLEAN_NOZZLE')
  self.assertIn('CLEAN_NOZZLE', self.text())
  self.assertIn('TEMP', self.text())
  self.click(700, 510)
  self.assertIn('Run macro?', self.text())
  self.assertEqual(self.click(700, 510)['command'], 'CLEAN_NOZZLE')
  self.host.request('open drybox-page 0')
  for label, command in (('PLA', 'DRY_PLA'), ('PETG', 'DRY_PETG'), ('Hold', 'DRY_HOLD'), ('Resume', 'DRY_RESUME'), ('Stop', 'DRY_STOP')):
   self.assertEqual(self.press(label)['command'], command)
  self.host.request('open dashboard-page 0')
  self.assertIn('NOZZLE', self.text())
 def test_studio_services(self): self.services(False)
 def test_operator_services(self): self.services(True)

if __name__ == '__main__':
 names = ('test_studio_services', 'test_operator_services')
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoServicesTests(name) for name in names)).wasSuccessful())
