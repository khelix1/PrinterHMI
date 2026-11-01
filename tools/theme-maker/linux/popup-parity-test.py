#!/usr/bin/env python3
"""Foreground dismissal and page/popup coverage through actual LVGL input."""
import argparse,base64,io,time,unittest,sys
from pathlib import Path
from PIL import Image
from native_preview import NativePreview
parser=argparse.ArgumentParser();parser.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[3]/'build-theme-maker-native/printerhmi-ui-host');args,remaining=parser.parse_known_args();HELPER=args.helper
class PopupParityTests(unittest.TestCase):
 def setUp(self):self.host=NativePreview(HELPER)
 def tearDown(self):self.host.close()
 def click(self,x,y):
  self.host.request(f'pointer {x} {y} 1');return self.host.request(f'pointer {x} {y} 0')
 def text(self):
  self.host.process.stdin.write(b'ui-text\n');self.host.process.stdin.flush();n=int(self.host.process.stdout.readline().split()[1]);return self.host._read(n,time.monotonic()+5).decode()
 def press(self,name):
  self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush();found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found,'Missing button '+name);return self.click((x1+x2)//2,(y1+y2)//2)
 def reveal(self,name):
  for _ in range(25):
   self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush();found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:]);self.assertTrue(found,'Missing button '+name)
   if 135 <= y1 and y2 <= 475:return
   self.host.request('wheel 500 350 '+('180' if y2>475 else '-180'))
  self.fail('Could not reveal '+name)
 def operator(self):
  self.host.request('open setup 0');self.press('Display');self.press('Operator');self.host.request('advance 300');self.press('Set up later')
 def ota(self):
  self.host.request('open settings-page 0')
  for _ in range(3):self.host.request('wheel 500 350 500')
  self.click(850,266);self.assertIn('Firmware releases',self.text())
 def transfer(self):
  self.ota();self.click(500,240);self.click(700,485);self.host.request('advance 300');self.assertIn('Simulating firmware transfer',self.text())
 def test_escape_cancels_transfer_before_closing_and_allows_reopen(self):
  self.transfer();self.host.request('key 27');self.assertIn('Simulation cancelled',self.text());self.host.request('key 27');self.assertNotIn('Simulation cancelled',self.text());self.host.request('advance 3000');self.click(850,266);self.assertIn('Firmware releases',self.text())
 def test_escape_confirmation_returns_to_browser_without_install(self):
  self.ota();self.click(500,240);self.assertIn('Switch to Stable v7.2.0',self.text());self.host.request('key 27');self.assertNotIn('Selected: Stable',self.text());self.assertIn('Firmware releases',self.text());self.host.request('key 27');self.assertNotIn('Firmware releases',self.text())
 def test_custom_url_keyboard_escape_and_reopen(self):
  self.ota();self.press('Custom URL');self.assertIn('Update from URL',self.text());self.host.request('key 27');self.assertNotIn('Update from URL',self.text());self.click(850,266);self.assertIn('Firmware releases',self.text())
 def test_lighting_escape_cleanup_and_reopen(self):
  for _ in range(4):
   self.host.request('open tools-page 0');self.press('Lighting');self.assertIn('Live case lighting',self.text());self.host.request('key 27');self.assertNotIn('Brightness:',self.text());self.press('Lighting');self.assertIn('Live case lighting',self.text());self.press('Close')
 def test_lightbox_escape_returns_to_page(self):
  self.host.request('open printer-page 0');self.click(330,180);self.assertIn('TAP ANYWHERE TO CLOSE',self.text());self.host.request('key 27');self.assertNotIn('TAP ANYWHERE TO CLOSE',self.text());self.assertEqual(self.host.hover(160,560),'pointer')
 def test_telemetry_dropdown_escape_is_repeatable(self):
  self.host.request('open telemetry-page 0')
  for _ in range(8):
   self.click(400,160);self.host.request('key 27');self.assertEqual(self.host.hover(825,160),'pointer')
  self.click(915,35);self.assertEqual(self.host.hover(90,560),'default');self.host.request('key 27');self.assertEqual(self.host.hover(90,560),'pointer')
 def test_all_page_routes_in_studio_and_operator(self):
  pages=['dashboard','printer','files','camera','tools','console','drybox','settings','network','chooser','telemetry','devices','macros','mesh','calibration']
  for style in ('studio','operator'):
   if style=='operator':self.operator()
   for page in pages:
    with self.subTest(style=style,page=page):
     result=self.host.request('open '+page+'-page 0',['state ready']);self.assertFalse(result['closed']);self.assertEqual(result['command'],'')
     image=Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1])));self.assertEqual(image.size,(1024,600));self.assertGreater(image.getchannel('A').getextrema()[1],0)
 def test_standalone_popup_escape_and_reopen_both_styles(self):
  for style in ('studio','operator'):
   if style=='operator':self.operator()
   for kind in ('nozzle','bed','fan','stop','toolhead','tune'):
    with self.subTest(style=style,kind=kind):
     for _ in range(3):
      self.host.request('open '+kind+' 0');closed=self.host.request('key 27');self.assertTrue(closed['closed']);self.assertEqual(closed['command'],'')
 def test_repeated_studio_operator_printer_rebuild(self):
  self.host.request('open printer-page 0')
  for theme in ('Operator','Studio dark','Operator','Studio dark'):
   self.host.request('open setup 0');self.press('Display');self.press(theme);self.host.request('advance 300');self.press('Set up later');self.host.request('open printer-page 0');self.assertIn('Nozzle',self.text());self.host.request('advance 300')
if __name__=='__main__':unittest.main(argv=[sys.argv[0],*remaining])
