#!/usr/bin/env python3
"""Real chooser/demo modal callbacks in both themes; no tablet boot mutation on desktop."""
import base64,importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class DemoTests(module.PopupParityTests):
 def test_demo_entry_confirmation_close_reopen_and_desktop_boundary(self):
  for theme in ('studio','operator'):
   if theme=='operator':self.operator()
   self.host.request('open chooser-page 0',['demo-mode-fixture 0']);self.press('Demo');self.assertIn('Try offline demo',self.text())
   self.press('Start demo');self.assertIn('Start offline demo?',self.text());self.press('Restart');self.assertIn('desktop already runs offline',self.text())
   self.press('Close');self.press('Demo');self.assertIn('Try offline demo',self.text());self.assertNotIn('Start offline demo?',self.text());self.host.request('key 27');self.assertNotIn('Try offline demo',self.text())
 def test_active_demo_scenarios_and_exit_confirmation(self):
  self.host.request('open chooser-page 0',['demo-mode-fixture 1']);self.press('Demo');self.assertIn('Offline demo',self.text())
  for _ in range(2):self.host.request('wheel 500 320 180')
  self.press('Paused');self.assertEqual(self.host.request('frame')['printState'],'paused')
  self.press('Exit demo');self.assertIn('Return to your printers?',self.text());self.press('Restart');self.assertIn('Desktop already runs offline',self.text());self.press('Close')
 def test_chooser_action_bounds_and_operator_screenshot(self):
  self.operator();result=self.host.request('open chooser-page 0',['demo-mode-fixture 0'])
  def bounds(name):
   self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush();values=list(map(int,self.host.process.stdout.readline().split()[1:]));self.assertTrue(values[0]);return values[1:]
  demo=bounds('Demo');manage=bounds('Manage printers');self.assertLess(demo[2],manage[0]);self.assertGreaterEqual(demo[1],100)
  self.press('Demo');self.assertIn('Try offline demo',self.text());self.press('Close')
  result=self.host.request('frame');Path('/tmp/demo-chooser-operator.png').write_bytes(base64.b64decode(result['image'].split(',')[1]))
  self.press('Demo');result=self.host.request('frame');Path('/tmp/demo-entry-operator.png').write_bytes(base64.b64decode(result['image'].split(',')[1]))
# Only run these new tests; base suite is run separately.
if __name__=='__main__':
 suite=unittest.TestSuite(DemoTests(name) for name in ('test_demo_entry_confirmation_close_reopen_and_desktop_boundary','test_chooser_action_bounds_and_operator_screenshot','test_active_demo_scenarios_and_exit_confirmation'))
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
