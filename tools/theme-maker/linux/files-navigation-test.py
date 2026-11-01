#!/usr/bin/env python3
"""Compare actual navigation-button pixels across a confirmed Files print transition."""
import base64,importlib.util,io,sys,unittest
from pathlib import Path
from PIL import Image
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class FilesNavigationTests(m.PopupParityTests):
 def nav_pixels(self,result):
  image=Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1]))).convert('RGB');parts=[]
  for name in ('Dashboard','Files'):
   self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush();found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found)
   parts.append(image.crop((x1+4,y1+4,x2-3,y2-3)).tobytes())
  return tuple(parts)
 def transition(self,operator,cancel):
  if operator:self.operator()
  expected=self.nav_pixels(self.host.request('open dashboard-page 0',['state ready']))
  original=self.nav_pixels(self.host.request('open files-page 0'));self.assertTrue(expected!=original)
  self.host.request('advance 25');self.press('Bracket.gcode');self.press('Start print' if operator else 'Print')
  if cancel:
   self.press('Cancel');self.press('Cancel');self.assertTrue(self.nav_pixels(self.host.request('frame'))==original)
  else:
   result=self.press('Start print');self.assertEqual(result['command'],'@nav:dashboard');self.assertEqual(result['printState'],'printing');self.assertTrue(self.nav_pixels(result)==expected,'Dashboard opened but the rail/dock still highlights Files')
 def test_studio_success_selects_dashboard(self):self.transition(False,False)
 def test_operator_success_selects_dashboard(self):self.transition(True,False)
 def test_studio_cancel_keeps_files(self):self.transition(False,True)
 def test_operator_cancel_keeps_files(self):self.transition(True,True)
if __name__=='__main__':
 names=['test_studio_success_selects_dashboard','test_operator_success_selects_dashboard','test_studio_cancel_keeps_files','test_operator_cancel_keeps_files']
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(FilesNavigationTests(name) for name in names)).wasSuccessful())
