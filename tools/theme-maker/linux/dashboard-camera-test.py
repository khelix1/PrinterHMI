#!/usr/bin/env python3
"""Dashboard camera layering, fullscreen return, and Printer navigation."""
import importlib.util, sys, unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DashboardCameraTests(m.PopupParityTests):
 def camera(self,operator):
  if operator: self.operator()
  self.host.request('scenario printing');self.host.request('open dashboard-page 0')
  toggle=(485,223) if operator else (230,170)
  self.click(*toggle);self.host.request('advance 500')
  self.assertIn('Thumbnail',self.text())
  for _ in range(3):
   self.host.request('advance 1100')
   self.host.process.stdin.write(b'bounds Thumbnail\n');self.host.process.stdin.flush()
   found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found)
   self.assertEqual(self.host.hover((x1+x2)//2,(y1+y2)//2),'pointer')
  self.press('Thumbnail');self.assertNotIn('LIVE CAMERA',self.text())
  self.click(*toggle);self.host.request('advance 300')
  # The video image is distinct from the toggle; tap its center.
  self.click(380 if operator else 150,340 if operator else 260)
  self.assertIn('Exit fullscreen',self.text())
  self.click(500,300);self.host.request('advance 300')
  self.assertIn('Thumbnail',self.text());self.assertNotIn('Exit fullscreen',self.text())
  self.press('Thumbnail')
  self.host.request('open printer-page 0')
  self.assertEqual(self.press('Camera')['command'],'@nav:camera')
 def test_studio(self): self.camera(False)
 def test_operator(self): self.camera(True)
if __name__=='__main__':
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DashboardCameraTests(n) for n in ('test_studio','test_operator'))).wasSuccessful())
