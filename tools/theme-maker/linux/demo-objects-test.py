#!/usr/bin/env python3
"""Use the actual Dashboard/Printer object dialogs and verify exhaustion/reset."""
import importlib.util,sys,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('popup_parity',Path(__file__).with_name('popup-parity-test.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DemoObjectTests(m.PopupParityTests):
 def objects(self,operator,page):
  if operator:self.operator()
  self.host.request('scenario printing');self.host.request('open '+page+'-page 0')
  name='Cancel object' if operator and page=='dashboard' else 'Object'
  for part in ('part_1','part_2'):
   self.press(name);self.assertIn(part,self.text());self.press('Exclude');result=self.press('Exclude');self.assertEqual(result['command'],'EXCLUDE_OBJECT NAME='+part)
  self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush();found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found)
  self.assertEqual(self.host.hover((x1+x2)//2,(y1+y2)//2),'default')
  self.host.request('scenario printing');self.assertEqual(self.host.hover((x1+x2)//2,(y1+y2)//2),'pointer')
 def test_studio_dashboard_objects(self):self.objects(False,'dashboard')
 def test_operator_dashboard_objects(self):self.objects(True,'dashboard')
 def test_studio_printer_objects(self):self.objects(False,'printer')
 def test_operator_printer_objects(self):self.objects(True,'printer')
if __name__=='__main__':
 names=['test_studio_dashboard_objects','test_operator_dashboard_objects','test_studio_printer_objects','test_operator_printer_objects']
 sys.exit(not unittest.TextTestRunner(verbosity=2).run(unittest.TestSuite(DemoObjectTests(name) for name in names)).wasSuccessful())
