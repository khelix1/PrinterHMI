#!/usr/bin/env python3
"""Shared offline Files behavior beyond the existing native Files regression cases."""
import importlib.util,sys,unittest,base64,io
from pathlib import Path
from PIL import Image
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoFilesTests(m.PopupParityTests):
 def search(self,text):
  self.click(200,150)
  for _ in range(64):self.host.request('key 8')
  for c in text:self.host.request('key '+str(ord(c)))
  self.host.request('key 10')
 def test_confirmed_sample_has_targets_and_advances(self):
  self.host.request('open files-page 0',['state ready']);self.host.request('advance 25');self.click(230,345);self.click(720,475)
  cancelled=self.click(350,430);self.assertEqual(cancelled['printState'],'ready')
  self.click(720,475);started=self.click(650,430);self.assertEqual(started['job'],'Bracket.gcode');self.assertEqual(started['nozzle'],210);self.assertEqual(started['bed'],60)
  self.host.request('simulation-run 1');self.assertGreater(self.host.request('advance 10000')['progress'],0)
 def test_search_includes_nested_paths_and_previews_differ(self):
  self.host.request('open files-page 0',['state ready']);self.host.request('advance 25');self.search('cube')
  self.assertEqual(self.text().count('Calibration cube.gcode'),2)
  self.click(230,345);self.assertIn('Calibration/Calibration cube.gcode',self.text());self.click(930,475)
  # Click a full-size file preview from the real row; the title/file binding is retained.
  self.click(68,250);self.assertIn('TAP ANYWHERE TO CLOSE',self.text());first=self.host.request('frame')['image'];self.click(400,300)
  self.search('tray');self.click(68,250);second=self.host.request('frame')['image'];self.assertNotEqual(first,second)
 def test_old_confirmation_cannot_start_new_printer(self):
  self.host.request('open files-page 0',['state ready']);self.host.request('advance 25');self.click(230,345);self.click(720,475)
  changed=self.host.request('frame',['select-profile 1']);self.assertEqual(changed['printer'],'Sermoon D1 #2')
  result=self.click(650,430);self.assertEqual(result['command'],'');self.assertNotEqual(result['printState'],'printing')
 def test_teardown_and_reopen_in_both_themes(self):
  for style in ('studio','operator'):
   if style=='operator':self.operator()
   for _ in range(3):
    self.host.request('open files-page 0',['state ready']);self.host.request('advance 25');self.host.request('wheel 300 350 300');self.host.request('close');self.host.request('open files-page 0');self.host.request('advance 25');self.assertIn('Calibration',self.text())
if __name__=='__main__':
 suite=unittest.TestSuite(DemoFilesTests(name) for name in ('test_confirmed_sample_has_targets_and_advances','test_search_includes_nested_paths_and_previews_differ','test_teardown_and_reopen_in_both_themes','test_old_confirmation_cannot_start_new_printer'))
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
