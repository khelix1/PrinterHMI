#!/usr/bin/env python3
"""Exercise actual firmware widgets through the Linux helper's pointer/key protocol."""
import argparse
import base64
import io
from pathlib import Path
import unittest
from PIL import Image
from native_preview import NativePreview
parser=argparse.ArgumentParser();parser.add_argument('--helper',type=Path,default=Path(__file__).resolve().parents[3]/'build-theme-maker-native/printerhmi-ui-host');args,remaining=parser.parse_known_args()
class NativeTests(unittest.TestCase):
    def setUp(self): self.host=NativePreview(args.helper.resolve())
    def tearDown(self): self.host.close()
    def click(self,x,y):
        self.host.request(f'pointer {x} {y} 1')
        return self.host.request(f'pointer {x} {y} 0')
    def type_number(self,value):
        for _ in range(3):self.host.request('key 8')
        for c in str(value):self.host.request('key '+str(ord(c)))
    def test_real_preset_callback_and_alpha(self):
        result=self.host.request('open nozzle 0')
        im=Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1])))
        self.assertEqual(im.size,(1024,600));self.assertLess(im.getpixel((0,0))[3],255)
        result=self.click(480,285)
        self.assertTrue(result['closed']);self.assertEqual(result['command'],'M104 S200');self.assertEqual(result['nozzle'],200)
    def test_native_custom_keyboard_range_and_command(self):
        self.host.request('open nozzle 0');self.click(650,180)
        self.type_number(999);result=self.host.request('key 10')
        self.assertFalse(result['closed']);self.assertEqual(result['command'],'')
        self.type_number(220);result=self.host.request('key 10')
        self.assertTrue(result['closed']);self.assertEqual(result['command'],'M104 S220')
    def test_nested_escape_and_back_footer(self):
        self.host.request('open nozzle 0');self.click(650,180)
        result=self.host.request('key 27');self.assertFalse(result['closed'])
        result=self.click(512,490);self.assertTrue(result['closed'])
    def test_real_bed_fan_and_repeated_cleanup(self):
        for kind in ['bed','fan','stop','toolhead']*8:
            self.assertFalse(self.host.request('open '+kind+' 0')['closed'])
            self.assertTrue(self.host.request('close')['closed'])
        self.host.request('open bed 0');result=self.click(480,285)
        self.assertEqual(result['command'],'M140 S60')
    def test_native_palette_override_and_tune(self):
        result=self.host.request('open nozzle 1',['color popup 123456'])
        im=Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1])))
        self.assertEqual(im.getpixel((800,110))[:3],(18,52,86))
        result=self.host.request('open tune 0',['factors 100 105'])
        self.assertEqual(result['flow'],105)
        self.host.request('pointer 400 328 1')
        result=self.host.request('pointer 440 328 0')
        self.assertTrue(result['command'].startswith('M220 S'))
        self.assertGreater(result['speed'],100)
        self.assertTrue(self.host.request('key 27')['closed'])
    def test_frame_poll_and_position_seed(self):
        result=self.host.request('open toolhead 0',['position 110 115 12.8 -0.05'])
        self.assertEqual(result['position'],[110,115,12.8]);self.assertEqual(result['zOffset'],-0.05)
        self.assertFalse(self.host.request('frame')['closed'])
        self.assertTrue(self.host.request('key 27')['closed'])
if __name__=='__main__':unittest.main(argv=[__file__,*remaining])
