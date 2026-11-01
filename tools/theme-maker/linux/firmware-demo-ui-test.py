#!/usr/bin/env python3
"""Exercise tablet demo UI guards in the host-only firmware-demo-ui build."""
import importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class TabletDemoTests(m.PopupParityTests):
 def test_tablet_demo_marker_disabled_navigation_and_failed_exit(self):
  for style in ('studio','operator'):
   if style=='operator':self.operator()
   self.host.request('open dashboard-page 0');self.assertIn('DEMO |',self.text())
   # Camera is available and routes through the normal page action in demo.
   self.host.process.stdin.write(b'bounds Camera\n');self.host.process.stdin.flush();found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found)
   self.assertEqual(self.host.hover((x1+x2)//2,(y1+y2)//2),'pointer')
   self.assertEqual(self.click((x1+x2)//2,(y1+y2)//2)['command'],'@nav:camera')
   self.host.request('open camera-page 0');self.host.request('advance 300');self.assertIn('DEMO | Sample camera view',self.text())
   selected = 'Front camera' if style == 'studio' else 'Side camera'
   target = 'Side camera' if style == 'studio' else 'Front camera'
   self.press(selected);self.assertIn('Select camera',self.text());self.press(target);self.host.request('advance 300');self.assertIn(target,self.text())
   self.press('Fullscreen');self.host.request('key 27');self.assertIn(target,self.text())
   self.host.request('open chooser-page 0');self.press('Demo');self.assertIn('Offline demo',self.text())
   for _ in range(2):self.host.request('wheel 500 320 180')
   self.press('Paused');self.assertEqual(self.host.request('frame')['printState'],'paused')
   self.press('Exit demo');self.assertIn('Return to your printers?',self.text());self.press('Restart')
   self.assertIn('No restart performed',self.text());self.assertEqual(self.host.request('frame')['printState'],'paused')
   self.press('Close')
 def test_tablet_live_entry_write_failure_stays_in_confirmation(self):
  self.host.request('open chooser-page 0',['demo-mode-fixture 0']);self.press('Demo');self.press('Start demo');self.press('Restart');self.assertIn('No restart performed',self.text());self.assertIn('Start offline demo?',self.text());self.press('Close')
if __name__=='__main__':
 suite=unittest.TestSuite(TabletDemoTests(name) for name in ('test_tablet_demo_marker_disabled_navigation_and_failed_exit','test_tablet_live_entry_write_failure_stays_in_confirmation'))
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
