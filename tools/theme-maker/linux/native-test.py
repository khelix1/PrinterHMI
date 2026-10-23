#!/usr/bin/env python3
"""Exercise actual firmware widgets through the Linux helper's pointer/key protocol."""
import argparse
import base64
import io
import time
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
    def test_native_tools_shell_click_contract(self):
        self.host.request('open tools-page 0')
        for point in [(50,40),(740,40),(40,95),(512,250),(10,510)]:
            self.assertEqual(self.click(*point)['command'],'','display-only shell/page area')
        self.assertEqual(self.click(400,40)['command'],'@printers')
        self.assertEqual(self.click(160,560)['command'],'@nav:printer')
        self.assertEqual(self.click(800,350)['command'],'@tool:Macros')
        self.assertTrue(self.host.request('close')['closed'])
        self.assertFalse(self.host.request('open tools-page 0')['closed'])
        self.assertTrue(self.host.request('close')['closed'])
    def test_hover_matches_real_widgets_without_clicks(self):
        self.host.request('open tools-page 0')
        for point in [(50,40),(740,40),(40,95),(512,250),(10,510)]:
            self.assertEqual(self.host.hover(*point),'default')
        for point in [(400,40),(160,560),(800,350),(920,35)]:
            self.assertEqual(self.host.hover(*point),'pointer')
        self.assertEqual(self.host.request('frame')['command'],'','hover must never invoke a callback')
        self.host.request('open nozzle 0')
        self.assertEqual(self.host.hover(480,285),'pointer')
        self.assertEqual(self.host.hover(40,40),'default')
        self.click(650,180)
        self.assertEqual(self.host.hover(400,200),'text')
    def test_console_command_history_and_close(self):
        self.host.request('open console-page 0')
        self.assertEqual(self.host.hover(80,480),'pointer')
        self.assertEqual(self.host.hover(800,350),'default')
        self.click(80,480)
        self.assertEqual(self.host.hover(400,120),'text')
        self.assertEqual(self.host.hover(160,560),'default','modal blocks underlying navigation')
        for c in 'STATUS': self.host.request('key '+str(ord(c)))
        result=self.host.request('key 10')
        self.assertEqual(result['command'],'STATUS');self.assertFalse(result['closed'])
        self.click(80,480);self.click(410,523)
        self.assertEqual(self.host.request('key 10')['command'],'STATUS','Previous restores actual command history')
        self.click(80,480);self.click(210,523)
        self.assertEqual(self.host.hover(80,480),'pointer','Close returns to Console')
        self.click(80,480);self.host.request('key 27')
        self.assertEqual(self.host.hover(80,480),'pointer','Escape cancels editor without closing page')
        self.assertEqual(self.click(560,560)['command'],'@nav:tools')
    def test_console_filter_search_and_repeated_teardown(self):
        for _ in range(4):
            original=self.host.request('open console-page 0')['image']
            self.assertNotEqual(self.click(360,165)['image'],original)
            self.click(640,165)
            self.click(510,165)
            self.assertEqual(self.host.hover(400,120),'text')
            for c in 'Klipper':self.host.request('key '+str(ord(c)))
            self.host.request('key 10')
            self.assertEqual(self.host.hover(80,480),'pointer')
            self.click(640,165)
            self.click(350,480)
            self.assertTrue(self.host.request('close')['closed'])
    def test_native_printer_click_contract_and_controls(self):
        self.host.request('open printer-page 0',['position 110 110 12.8 0','state printing'])
        for point in [(300,475),(580,475),(400,400),(800,365),(50,40)]:
            self.assertEqual(self.host.hover(*point),'default')
            self.assertEqual(self.click(*point)['command'],'')
        for point in [(750,130),(750,280),(700,390),(130,475),(590,350),(560,420)]:
            self.assertEqual(self.host.hover(*point),'pointer')
        self.click(750,130);result=self.click(480,285)
        self.assertEqual(result['command'],'M104 S200');self.assertFalse(result['closed'])
        self.assertEqual(self.host.hover(130,475),'pointer')
        self.click(590,350)
        self.assertEqual(self.host.hover(130,475),'default','Tune blocks page actions')
        self.host.request('pointer 400 328 1');result=self.host.request('pointer 440 328 0')
        self.assertTrue(result['command'].startswith('M220 S'))
        self.click(750,400);self.click(590,350)
        self.assertEqual(self.host.hover(130,475),'default','Tune reopens after Close')
        self.host.request('key 27');self.assertEqual(self.host.hover(130,475),'pointer')
    def test_native_printer_pause_recovery_resume_and_stop(self):
        self.host.request('open printer-page 0')
        result=self.click(450,475);self.assertEqual(result['command'],'PAUSE');self.assertEqual(result['printState'],'paused')
        self.assertEqual(self.host.hover(450,475),'default');self.assertEqual(self.host.hover(580,475),'pointer')
        self.click(580,475);result=self.click(660,360)
        self.assertEqual(result['command'],'RESUME');self.assertEqual(result['printState'],'printing')
        self.click(830,543);self.assertEqual(self.host.hover(450,475),'pointer')
        self.click(900,475);result=self.click(360,420)
        self.assertEqual(result['command'],'');self.assertEqual(result['printState'],'printing')
        self.click(900,475);result=self.click(670,420)
        self.assertEqual(result['command'],'CANCEL_PRINT');self.assertEqual(result['printState'],'cancelled');self.assertFalse(result['closed'])
        self.assertEqual(self.host.hover(900,475),'default');self.assertEqual(self.host.hover(300,475),'pointer')
    def test_native_printer_object_confirmation_and_lightbox(self):
        self.host.request('open printer-page 0')
        self.click(720,475);self.click(700,520)
        self.assertEqual(self.host.hover(130,475),'default')
        result=self.click(650,420);self.assertEqual(result['command'],'EXCLUDE_OBJECT NAME=part_1');self.assertFalse(result['closed'])
        self.host.request('key 27')
        self.click(330,180);self.assertEqual(self.host.hover(160,560),'pointer','fullscreen tap closes everywhere')
        result=self.click(330,180);self.assertFalse(result['closed']);self.assertEqual(self.host.hover(300,475),'default')
        self.click(130,475);self.assertEqual(self.host.hover(160,560),'default')
        self.host.request('key 27');self.assertEqual(self.host.hover(130,475),'pointer')
        self.click(560,420);self.assertEqual(self.host.hover(130,475),'default','filament status opens real popup')
        self.host.request('key 27');self.assertEqual(self.host.hover(130,475),'pointer')
    def test_printer_page_state_seed_navigation_and_cleanup(self):
        for state in ['printing','paused','cancelled','shutdown']*3:
            result=self.host.request('open printer-page 0',['state '+state,'job Cable organizer.gcode'])
            self.assertEqual(result['printState'],state)
            self.click(750,130);self.host.request('key 27')
            self.assertFalse(self.host.request('frame')['closed'])
            self.assertEqual(self.click(560,560)['command'],'@nav:tools')
            self.assertTrue(self.host.request('close')['closed'])
    def test_dashboard_readings_are_inert_and_controls_real(self):
        self.host.request('open dashboard-page 0')
        for point in [(800,150),(800,300),(890,385),(100,450),(300,450),(500,450),(700,450),(900,450),(500,250)]:
            self.assertEqual(self.host.hover(*point),'default','telemetry is display-only')
            self.assertEqual(self.click(*point)['command'],'')
            self.assertEqual(self.host.hover(420,385),'pointer','readings must not open a modal')
        for point in [(420,385),(540,385),(670,385),(150,260),(400,40)]:
            self.assertEqual(self.host.hover(*point),'pointer')
        self.click(150,260)
        self.assertEqual(self.host.hover(800,150),'pointer','real thumbnail opens fullscreen')
        self.assertFalse(self.click(150,260)['closed'])
        self.assertEqual(self.host.hover(800,150),'default','tap closes fullscreen back to Dashboard')
    def test_dashboard_pause_resume_and_stop_confirmation(self):
        self.host.request('open dashboard-page 0')
        result=self.click(420,385);self.assertEqual(result['command'],'PAUSE');self.assertEqual(result['printState'],'paused')
        self.click(420,385)
        self.assertEqual(self.host.hover(160,560),'default','Resume opens modal recovery and blocks navigation')
        result=self.click(660,360);self.assertEqual(result['command'],'RESUME');self.assertEqual(result['printState'],'printing')
        self.click(830,543);self.assertEqual(self.host.hover(420,385),'pointer')
        self.click(540,385);result=self.click(360,420);self.assertEqual(result['command'],'');self.assertEqual(result['printState'],'printing')
        self.click(540,385);result=self.click(670,420)
        self.assertEqual(result['command'],'CANCEL_PRINT');self.assertEqual(result['printState'],'cancelled');self.assertFalse(result['closed'])
        for point in [(420,385),(540,385),(670,385)]:
            self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
    def test_dashboard_object_flow_and_popup_escape(self):
        self.host.request('open dashboard-page 0')
        self.click(670,385);self.click(700,520)
        self.assertEqual(self.host.hover(420,385),'default')
        result=self.click(650,420);self.assertEqual(result['command'],'EXCLUDE_OBJECT NAME=part_1')
        self.host.request('key 27');self.assertEqual(self.host.hover(420,385),'pointer')
        self.click(540,385);self.host.request('key 27')
        self.assertEqual(self.host.hover(420,385),'pointer')
        self.click(150,260);self.host.request('key 27')
        self.assertEqual(self.host.hover(800,150),'default');self.assertEqual(self.host.hover(420,385),'pointer')
    def test_dashboard_progress_job_and_page_cleanup(self):
        for kind in ['dashboard-page','printer-page','dashboard-page','console-page']*3:
            result=self.host.request('open '+kind+' 0',['state printing','progress 0','job Cable organizer.gcode'])
            self.assertEqual(result['progress'],0);self.assertFalse(result['closed'])
            if kind=='dashboard-page':
                self.click(150,260);self.host.request('key 27')
                self.assertEqual(self.click(210,560)['command'],'@nav:printer')
            self.assertTrue(self.host.request('close')['closed'])
        result=self.host.request('open dashboard-page 0',['progress 37.5','state cancelled'])
        self.assertEqual(result['progress'],37.5);self.assertEqual(self.host.hover(420,385),'default')
    def test_files_folder_search_empty_results_and_up(self):
        self.host.request('open files-page 0',['state ready']);self.host.request('advance 25')
        self.assertEqual(self.host.hover(70,150),'default')
        self.click(230,240);self.assertEqual(self.host.hover(70,150),'pointer')
        self.click(230,250);self.assertEqual(self.host.hover(720,475),'pointer')
        self.click(930,475);self.assertEqual(self.host.hover(720,475),'default')
        self.click(70,150);self.assertEqual(self.host.hover(70,150),'default')
        self.click(200,150);self.assertEqual(self.host.hover(400,120),'text')
        for c in 'cube':self.host.request('key '+str(ord(c)))
        self.host.request('key 10');self.assertEqual(self.host.hover(70,150),'pointer')
        self.click(230,250);self.assertEqual(self.host.hover(720,475),'pointer')
        self.click(200,150)
        for _ in range(4):self.host.request('key 8')
        for c in 'no-such-file':self.host.request('key '+str(ord(c)))
        self.host.request('key 10')
        self.assertEqual(self.host.hover(230,250),'default','empty search has no file target')
        self.click(70,150);self.assertEqual(self.host.hover(70,150),'default')
        self.assertEqual(self.host.hover(230,345),'pointer')
    def test_files_print_readiness_confirmation_and_job_state(self):
        self.host.request('open files-page 0',['state printing']);self.host.request('advance 25')
        self.click(230,345);self.assertEqual(self.host.hover(720,475),'default')
        self.assertEqual(self.click(720,475)['command'],'','busy printer cannot start another file')
        self.host.request('open files-page 0',['state ready']);self.host.request('advance 25')
        self.click(230,345);self.click(720,475)
        self.assertEqual(self.host.hover(160,560),'default','confirmation blocks navigation')
        result=self.click(350,430);self.assertEqual(result['printState'],'ready');self.assertEqual(result['command'],'')
        self.click(720,475);result=self.click(650,430)
        self.assertEqual(result['command'],'@nav:dashboard');self.assertEqual(result['job'],'Bracket.gcode')
        self.assertEqual(result['printState'],'printing');self.assertEqual(result['progress'],0)
        self.assertEqual(self.click(650,430)['command'],'','retired confirmation cannot dispatch again')
    def test_files_wheel_thumbnail_and_modal_isolation(self):
        self.host.request('open files-page 0',['state ready']);self.host.request('advance 25')
        self.click(68,345)
        self.assertEqual(self.host.hover(70,150),'pointer','thumbnail opens real fullscreen')
        self.click(400,300);self.assertEqual(self.host.hover(70,150),'default')
        baseline=self.host.request('frame')['image']
        self.assertNotEqual(self.host.request('wheel 300 380 400')['image'],baseline)
        self.host.request('wheel 300 380 -600')
        self.assertEqual(self.host.hover(230,240),'pointer')
        self.click(200,150)
        self.host.request('wheel 40 250 500')
        self.host.request('key 27');self.assertEqual(self.host.hover(230,240),'pointer')
        self.click(230,240);self.assertEqual(self.host.hover(70,150),'pointer','modal backdrop blocked wheel; folder remains at the top')
    def test_files_metadata_scroll_close_and_refresh(self):
        self.host.request('open files-page 0',['state ready']);self.host.request('advance 25')
        self.click(350,150);self.click(350,150);self.click(230,345)
        self.assertEqual(self.host.hover(830,475),'pointer','long metadata exposes Details')
        self.click(830,475);baseline=self.host.request('frame')['image']
        self.assertNotEqual(self.host.request('wheel 400 300 400')['image'],baseline)
        self.click(750,480);self.assertEqual(self.host.hover(720,475),'pointer')
        self.click(520,150);self.host.request('frame')
        self.assertEqual(self.host.hover(720,475),'pointer','refresh preserves selected detail')
        self.click(930,475);self.assertEqual(self.host.hover(720,475),'default')
    def test_files_repeated_teardown_with_search_and_metadata(self):
        for _ in range(5):
            self.host.request('open files-page 0',['state ready']);self.host.request('advance 25')
            self.click(230,345);self.click(200,150)
            self.assertTrue(self.host.request('close')['closed'])
            self.host.request('open files-page 0')
            self.assertEqual(self.host.hover(70,150),'default')
            self.assertEqual(self.host.request('frame')['job'],'Spool holder.gcode')
            self.assertEqual(self.click(80,560)['command'],'@nav:dashboard')
    def camera_ready(self):
        for _ in range(2):
            time.sleep(.11);result=self.host.request('frame')
        return result
    def camera_pixels(self,result):
        return Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1]))).crop((260,160,760,410)).tobytes()
    def camera_edit(self,point,old,text):
        self.click(*point)
        for _ in range(len(old)+2):self.host.request('key 8')
        for c in text:self.host.request('key '+str(ord(c)))
        return self.host.request('key 10')
    def test_camera_real_controls_fullscreen_and_escape(self):
        self.host.request('open camera-page 0');self.camera_ready()
        self.assertEqual(self.host.hover(500,300),'default','normal image has no tap action')
        self.assertEqual(self.click(500,300)['command'],'')
        for point in [(200,460),(390,460),(550,460),(850,460)]:self.assertEqual(self.host.hover(*point),'pointer')
        self.click(390,460);self.assertEqual(self.host.hover(200,560),'default')
        self.click(350,490);self.assertEqual(self.host.hover(500,300),'default')
        self.click(550,460);self.assertEqual(self.host.hover(200,560),'default')
        self.click(750,480);self.click(850,460)
        self.assertEqual(self.host.hover(500,300),'pointer')
        self.assertEqual(self.click(500,300)['command'],'');self.assertEqual(self.host.hover(500,300),'default')
        self.click(850,460);self.host.request('key 27')
        self.assertEqual(self.host.hover(500,300),'default','Escape restores page without deleting camera surface')
        self.assertEqual(self.click(80,560)['command'],'@nav:dashboard')
    def test_camera_view_transform_picker_and_default(self):
        self.host.request('open camera-page 0');baseline=self.camera_pixels(self.camera_ready())
        self.click(390,460);self.click(680,215);rotated=self.camera_pixels(self.camera_ready())
        self.assertNotEqual(rotated,baseline,'real rotation changes rendered frame')
        self.click(390,460);self.click(350,354);mirrored=self.camera_pixels(self.camera_ready())
        self.assertNotEqual(mirrored,rotated,'real pixel mirroring changes frame')
        self.click(390,460);self.click(650,490)
        self.assertEqual(self.camera_pixels(self.camera_ready()),baseline,'Reset restores initial view')
        self.click(550,460);self.click(400,245);side=self.camera_pixels(self.camera_ready())
        self.assertNotEqual(side,baseline,'picker selects a distinct sample feed')
        self.host.request('open camera-page 0')
        self.assertEqual(self.camera_pixels(self.camera_ready()),side,'selected default survives page recreation')
    def test_camera_setup_keyboard_test_find_and_cancel(self):
        self.host.request('open camera-page 0');self.click(200,460)
        self.assertEqual(self.host.hover(400,210),'text')
        self.camera_edit((400,210),'Front camera','Desk camera')
        self.assertEqual(self.host.hover(750,540),'pointer','Done returns from actual field keyboard')
        before=self.host.request('frame')['image'];self.click(390,360);self.camera_ready()
        self.assertNotEqual(self.host.request('frame')['image'],before,'test result updates native setup')
        self.click(200,360);self.camera_ready()
        self.click(750,540);self.camera_ready()
        self.assertEqual(self.host.hover(390,460),'pointer','Use camera returns to viewer')
        self.click(200,460);self.click(400,210);self.host.request('key 27')
        self.assertEqual(self.host.hover(750,540),'pointer','Escape cancels nested keyboard')
        self.host.request('key 27');self.assertEqual(self.host.hover(390,460),'pointer','Escape cancels screen-local setup')
    def test_camera_failure_retry_and_local_discovery_recovery(self):
        self.host.request('open camera-page 0');baseline=self.camera_pixels(self.camera_ready())
        self.click(200,460)
        self.camera_edit((400,300),'http://simulation.invalid/camera/1','http://fail.invalid')
        self.click(750,540);self.camera_ready()
        self.assertEqual(self.host.hover(100,178),'pointer','failed sample feed exposes Retry')
        self.assertEqual(self.click(100,178)['command'],'');self.camera_ready()
        self.click(200,460);self.click(200,360);self.camera_ready();self.click(750,540)
        self.assertEqual(self.camera_pixels(self.camera_ready()),baseline,'Find restores local sample cameras')
    def test_camera_remove_confirmation_empty_catalog_and_restore(self):
        self.host.request('open camera-page 0');self.camera_ready();self.click(200,460)
        self.click(850,360);self.assertEqual(self.host.hover(750,540),'default')
        self.click(350,430);self.assertEqual(self.host.hover(750,540),'pointer','Keep camera returns to setup')
        self.click(850,360);self.click(650,430)
        self.click(390,120);self.click(850,360);self.click(650,430);self.click(750,540);self.camera_ready()
        self.click(390,460)
        self.assertEqual(self.host.hover(200,560),'pointer','empty catalog reports status instead of opening View')
        self.click(200,460);self.click(200,360);self.camera_ready();self.click(750,540);self.camera_ready()
        self.click(390,460);self.assertEqual(self.host.hover(200,560),'default','restored catalog opens View')
    def test_camera_repeated_teardown_with_setup_and_fullscreen(self):
        for _ in range(4):
            self.host.request('open camera-page 0');self.camera_ready();self.click(850,460)
            self.assertTrue(self.host.request('close')['closed'])
            self.host.request('open camera-page 0');self.click(200,460);self.click(400,210)
            self.assertTrue(self.host.request('close')['closed'])
            self.host.request('open tools-page 0');self.assertEqual(self.host.hover(800,350),'pointer')
    def test_camera_cancelled_test_and_find_can_restart(self):
        self.host.request('open camera-page 0');self.click(200,460)
        def status():
            result=self.camera_ready()
            return Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1]))).crop((40,390,985,424)).tobytes()
        for action in [(390,360),(200,360)]:
            self.click(*action);expected=status()
            self.click(250,540);self.click(200,460);self.click(*action);self.click(250,540)
            self.click(200,460);self.click(*action)
            self.assertEqual(status(),expected,'cancelled operation must not leave test/discovery busy forever')
        self.click(250,540);self.assertEqual(self.host.hover(390,460),'pointer')
    def test_dashboard_camera_toggle_and_fullscreen_handoff(self):
        self.host.request('open dashboard-page 0')
        self.click(230,170);self.camera_ready()
        self.click(230,170)
        self.assertEqual(self.host.hover(800,150),'default','live image must not cover Thumbnail toggle')
        self.click(230,170);self.camera_ready();self.click(150,260)
        self.assertEqual(self.host.hover(800,150),'pointer','Dashboard live view opens real Camera fullscreen')
        self.host.request('key 27')
        self.assertEqual(self.host.hover(390,460),'pointer');self.assertEqual(self.host.hover(500,300),'default')
        self.assertEqual(self.click(560,560)['command'],'@nav:tools')
    def test_frame_poll_and_position_seed(self):
        result=self.host.request('open toolhead 0',['position 110 115 12.8 -0.05'])
        self.assertEqual(result['position'],[110,115,12.8]);self.assertEqual(result['zOffset'],-0.05)
        self.assertFalse(self.host.request('frame')['closed'])
        self.assertTrue(self.host.request('key 27')['closed'])
    def drybox_crop(self,result,box=(20,80,1005,440)):
        return Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1]))).crop(box).tobytes()
    def test_drybox_real_controls_and_inert_telemetry(self):
        self.assertFalse(self.host.request('open drybox-page 0')['closed'])
        for point in [(100,220),(500,270),(820,220),(820,350),(300,100)]:
            self.assertEqual(self.host.hover(*point),'default')
            self.assertEqual(self.click(*point)['command'],'')
        for x in [120,315,510,710,905]:self.assertEqual(self.host.hover(x,478),'pointer')
        self.assertEqual(self.click(450,560)['command'],'@nav:camera')
        self.assertFalse(self.host.request('key 27')['closed'])
    def test_drybox_programs_confirmed_state_and_duplicate_guard(self):
        idle=self.host.request('open drybox-page 0')
        pla=self.click(120,478);self.assertEqual(pla['command'],'DRY_PLA')
        self.assertNotEqual(self.drybox_crop(idle),self.drybox_crop(pla))
        duplicate=self.click(120,478);self.assertEqual(duplicate['command'],'')
        self.assertEqual(self.drybox_crop(pla),self.drybox_crop(duplicate))
        petg=self.click(315,478);self.assertEqual(petg['command'],'DRY_PETG')
        self.assertNotEqual(self.drybox_crop(pla),self.drybox_crop(petg))
        hold=self.click(510,478);self.assertEqual(hold['command'],'DRY_HOLD')
        resume=self.click(710,478);self.assertEqual(resume['command'],'DRY_RESUME')
        self.assertEqual(self.drybox_crop(petg),self.drybox_crop(resume))
        stop=self.click(905,478);self.assertEqual(stop['command'],'DRY_STOP')
        self.assertEqual(self.drybox_crop(idle),self.drybox_crop(stop))
    def test_drybox_offline_and_shutdown_disable_programs(self):
        for state in ['offline','shutdown','error']:
            self.host.request('open drybox-page 0',['state '+state])
            for x in [120,315,510,710,905]:
                self.assertEqual(self.host.hover(x,478),'default')
                self.assertEqual(self.click(x,478)['command'],'')
        self.host.request('frame',['state printing'])
        self.assertEqual(self.host.hover(120,478),'pointer')
        self.assertEqual(self.click(120,478)['command'],'DRY_PLA')
    def test_drybox_failed_send_keeps_program_and_can_retry(self):
        self.host.request('open drybox-page 0');pla=self.click(120,478)
        self.host.request('frame',['drybox-fail 1']);failed=self.click(315,478)
        self.assertEqual(failed['command'],'');self.assertEqual(self.host.hover(930,547),'pointer')
        # Real persistent error notice can be dismissed without activating Stop.
        dismissed=self.click(930,547);self.assertEqual(dismissed['command'],'')
        self.assertEqual(self.drybox_crop(pla),self.drybox_crop(dismissed))
        self.host.request('frame',['drybox-fail 0']);retry=self.click(315,478)
        self.assertEqual(retry['command'],'DRY_PETG')
        self.assertNotEqual(self.drybox_crop(pla),self.drybox_crop(retry))
    def test_drybox_telemetry_and_repeated_page_cleanup(self):
        base=self.host.request('open drybox-page 0')
        humid=self.host.request('frame',['drybox-sensors 31 32 52'])
        self.assertNotEqual(self.drybox_crop(base),self.drybox_crop(humid))
        active=self.click(315,478)
        for page in ['tools','camera','dashboard','files','printer','console']*2:
            self.assertFalse(self.host.request('open '+page+'-page 0')['closed'])
            restored=self.host.request('open drybox-page 0')
            self.assertEqual(self.drybox_crop(active),self.drybox_crop(restored))
        self.assertTrue(self.host.request('close')['closed'])
    def test_drybox_real_notice_dismiss_and_escape(self):
        self.host.request('open drybox-page 0',['notify 1'])
        accepted=self.click(120,478);self.assertEqual(accepted['command'],'DRY_PLA')
        self.assertEqual(self.host.hover(930,547),'pointer')
        dismissed=self.click(930,547);self.assertEqual(dismissed['command'],'')
        duplicate=self.click(120,478);self.assertEqual(duplicate['command'],'')
        self.assertNotEqual(self.drybox_crop(dismissed,(490,325,1000,580)),self.drybox_crop(duplicate,(490,325,1000,580)))
        self.assertFalse(self.host.request('key 27')['closed'])
        self.assertEqual(self.host.hover(905,478),'pointer')
        self.assertEqual(self.click(905,478)['command'],'DRY_STOP')
    def settings_at(self,scroll=0):
        result=self.host.request('open settings-page 0')
        while scroll:
            step=min(scroll,500);result=self.host.request('wheel 500 350 '+str(step));scroll-=step
        return result
    def settings_crop(self,result,box=(20,150,1000,500)):
        return self.drybox_crop(result,box)
    def test_settings_native_rows_scroll_and_workflow_routes(self):
        self.settings_at()
        for point in [(60,220),(512,100),(970,230)]:
            self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
        self.assertEqual(self.host.hover(750,230),'pointer')
        self.settings_at(1110)
        for point in [(80,158),(900,375),(100,437)]:
            self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
        self.settings_at(480);self.assertEqual(self.click(850,446)['command'],'@nav:network')
        self.settings_at(1500);self.assertEqual(self.click(850,266)['command'],'@setting:ota')
        self.settings_at(1710);self.assertEqual(self.click(850,198)['command'],'@setting:setup')
    def test_settings_brightness_live_and_page_persistence(self):
        before=self.settings_at();after=self.click(750,230)
        self.assertNotEqual(self.settings_crop(before),self.settings_crop(after))
        im=Image.open(io.BytesIO(base64.b64decode(after['image'].split(',')[1])))
        original=Image.open(io.BytesIO(base64.b64decode(before['image'].split(',')[1])))
        self.assertLess(im.getpixel((10,10))[0],original.getpixel((10,10))[0])
        self.host.request('open tools-page 0');restored=self.settings_at()
        self.assertEqual(self.settings_crop(after,(940,205,998,250)),self.settings_crop(restored,(940,205,998,250)))
    def test_settings_storage_and_display_failure_retry(self):
        before=self.settings_at();self.host.request('frame',['settings-fail 0 1'])
        failed=self.click(750,230);self.assertEqual(failed['command'],'')
        self.assertEqual(self.settings_crop(before,(940,205,998,250)),self.settings_crop(failed,(940,205,998,250)))
        self.host.request('key 27');self.host.request('frame',['settings-fail 1 0'])
        self.click(500,290);self.host.request('key 27')
        self.host.request('frame',['settings-fail 0 0']);success=self.click(500,290)
        self.assertNotEqual(self.settings_crop(before,(850,270,990,330)),self.settings_crop(success,(850,270,990,330)))
        # Brightness can be retried after a failed physical-display request.
        self.assertNotEqual(self.settings_crop(before),self.settings_crop(self.click(750,230)))
    def test_settings_sleep_first_touch_wakes_without_navigation(self):
        self.settings_at();self.click(500,290)
        asleep=self.host.request('advance 301000')
        im=Image.open(io.BytesIO(base64.b64decode(asleep['image'].split(',')[1])))
        self.assertEqual(im.getpixel((500,300))[:3],(0,0,0))
        self.assertEqual(self.host.hover(90,560),'pointer')
        woke=self.click(90,560);self.assertEqual(woke['command'],'')
        self.assertNotEqual(self.settings_crop(asleep),self.settings_crop(woke))
        self.assertEqual(self.click(90,560)['command'],'@nav:dashboard')
    def test_settings_appearance_rebuild_preserves_scroll(self):
        self.settings_at(240);self.click(500,193)
        default=self.host.request('frame');selected=self.click(300,225);self.host.request('frame')
        self.assertNotEqual(self.settings_crop(default),self.settings_crop(selected))
        self.host.request('key 27');self.assertEqual(self.host.hover(500,193),'pointer')
        self.click(500,263);comfortable=self.host.request('frame');compact=self.click(300,240)
        self.assertNotEqual(self.settings_crop(comfortable),self.settings_crop(compact))
        self.host.request('key 27');self.assertEqual(self.host.hover(500,330),'pointer')
    def test_settings_accessibility_native_toggle_reopen_cleanup(self):
        self.settings_at(240);self.click(500,330);off=self.host.request('frame')
        on=self.click(300,160);self.host.request('frame')
        self.assertNotEqual(self.settings_crop(off),self.settings_crop(on))
        self.host.request('key 27');self.assertFalse(self.host.request('frame')['closed'])
        self.settings_at(240);self.click(500,330);self.click(300,160);self.host.request('key 27')
        self.assertFalse(self.settings_at()['closed'])
    def test_settings_real_theme_chooser_and_shell_rebuild(self):
        self.settings_at();self.click(500,360);self.click(300,519)
        self.assertFalse(self.host.request('key 27')['closed'])
        self.host.request('key 27');self.click(500,360)
        operator=self.click(300,230);operator=self.host.request('frame')
        self.assertEqual(self.host.hover(80,106),'pointer','Operator shell uses its actual left rail')
        self.click(700,480);studio=self.click(650,230);studio=self.host.request('frame')
        self.assertNotEqual(self.settings_crop(operator),self.settings_crop(studio))
        self.assertEqual(self.host.hover(90,560),'pointer','Studio restores its actual bottom dock')
    def test_settings_notification_cancel_save_and_cross_page_behavior(self):
        original=self.settings_at(480);self.click(500,233);self.click(300,230);self.click(300,519)
        self.assertEqual(self.settings_crop(original,(850,205,990,260)),self.settings_crop(self.host.request('frame'),(850,205,990,260)))
        self.click(500,233);self.click(300,230);self.click(700,519)
        changed=self.host.request('frame');self.assertNotEqual(self.settings_crop(original,(850,205,990,260)),self.settings_crop(changed,(850,205,990,260)))
        self.host.request('open drybox-page 0');self.click(120,478)
        self.assertEqual(self.host.hover(930,547),'pointer','Saved notification level applies to real Drybox notice cards')
        self.assertEqual(self.click(930,547)['command'],'')
    def test_settings_timezone_native_selection_and_failed_save(self):
        original=self.settings_at(810);self.click(500,255)
        self.host.request('frame',['settings-fail 1 0']);failed=self.click(300,165)
        self.assertEqual(failed['command'],'');self.assertEqual(self.host.hover(512,510),'pointer')
        self.host.request('frame',['settings-fail 0 0']);changed=self.click(300,165)
        self.assertNotEqual(self.settings_crop(original,(700,220,990,285)),self.settings_crop(changed,(700,220,990,285)))
        self.assertFalse(changed['closed'])
    def test_settings_event_history_about_and_repeated_popup_cleanup(self):
        for _ in range(3):
            self.settings_at(480);self.click(850,303)
            self.assertEqual(self.host.hover(230,529),'pointer')
            self.click(230,529);self.click(510,529)
            self.assertFalse(self.click(790,529)['closed'])
            self.settings_at(3000);self.assertEqual(self.host.hover(100,240),'default')
            self.click(850,445);self.assertFalse(self.host.request('key 27')['closed'])
        self.assertTrue(self.host.request('close')['closed'])
    def test_settings_plain_backup_restore_remove_confirmations(self):
        self.settings_at(1110);self.click(850,238);base=self.host.request('frame')
        missing=self.click(300,325);self.assertNotEqual(self.settings_crop(base),self.settings_crop(missing))
        self.click(300,260);self.assertFalse(self.host.request('key 27')['closed'])
        self.click(300,325);self.assertEqual(self.host.hover(700,500),'pointer')
        self.click(700,500);self.assertFalse(self.host.request('key 27')['closed'])
        self.click(500,389);self.click(700,460)
        self.click(300,325);self.assertEqual(self.host.hover(700,500),'default')
        self.assertFalse(self.click(850,529)['closed'])
    def test_settings_encrypted_backup_validation_and_worker_lifecycle(self):
        initial=self.settings_at();self.settings_at(1110);self.click(850,238);self.click(720,260)
        blank=self.host.request('frame');short=self.host.request('key 10')
        self.assertNotEqual(self.settings_crop(blank),self.settings_crop(short))
        for ch in 'samplepass12':self.host.request('key '+str(ord(ch)))
        mismatch=self.host.request('key 10');self.assertNotEqual(self.settings_crop(short),self.settings_crop(mismatch))
        self.click(300,220)
        for ch in 'samplepass12':self.host.request('key '+str(ord(ch)))
        progress=self.host.request('key 10');done=self.host.request('advance 500')
        self.assertNotEqual(self.settings_crop(progress),self.settings_crop(done))
        self.host.request('advance 200');self.assertEqual(self.host.hover(777,460),'pointer')
        self.click(777,460);self.click(850,529)
        # Change live preferences after the snapshot, then restore and reboot.
        self.settings_at();self.click(750,230)
        self.settings_at(1110);self.click(850,238);self.click(720,325)
        for ch in 'wrongpass1234':self.host.request('key '+str(ord(ch)))
        self.host.request('key 10');wrong=self.host.request('advance 500')
        for ch in 'samplepass12':self.host.request('key '+str(ord(ch)))
        self.host.request('key 10');verified=self.host.request('advance 500')
        self.assertNotEqual(self.settings_crop(wrong),self.settings_crop(verified))
        self.assertEqual(self.host.hover(700,500),'pointer')
        self.click(700,500);self.host.request('key 27');self.host.request('key 27')
        self.settings_at(1710);self.click(850,340)
        self.assertEqual(self.click(700,424)['command'],'@nav:dashboard')
        restored=self.settings_at()
        self.assertEqual(self.settings_crop(initial,(940,205,998,250)),self.settings_crop(restored,(940,205,998,250)))
    def test_settings_reboot_preserves_preferences_and_factory_reset_routes(self):
        before=self.settings_at();self.click(500,290);saved=self.host.request('frame')
        self.settings_at(1710);self.click(850,340);self.click(300,424)
        self.assertFalse(self.host.request('frame')['closed'])
        self.click(850,340);reboot=self.click(700,424)
        self.assertEqual(reboot['command'],'@nav:dashboard')
        restored=self.settings_at();self.assertEqual(self.settings_crop(saved,(850,270,990,330)),self.settings_crop(restored,(850,270,990,330)))
        self.assertNotEqual(self.settings_crop(before,(850,270,990,330)),self.settings_crop(restored,(850,270,990,330)))
        self.settings_at(1710);self.click(850,340);self.assertEqual(self.click(700,424)['command'],'@nav:dashboard','A second simulated reboot has a new controller lifetime')
        self.settings_at(1710);self.click(850,478)
        self.host.request('frame',['settings-fail 1 0']);self.assertEqual(self.click(700,424)['command'],'')
        self.host.request('frame',['settings-fail 0 0']);self.assertEqual(self.click(700,424)['command'],'@native-reset')
    def network_crop(self,result,box=(20,150,1005,500)):
        return self.drybox_crop(result,box)
    def network_scan(self,mode=0,connect=0):
        self.host.request('open network-page 0',[f'network-fixture {mode} {connect}'])
        return self.click(690,110)
    def test_network_real_click_contract_and_navigation(self):
        self.assertFalse(self.host.request('open network-page 0')['closed'])
        for point in [(80,100),(100,310),(100,460),(350,310),(350,365),(350,425),(350,485),(800,270),(800,340)]:
            self.assertEqual(self.host.hover(*point),'default')
            self.assertEqual(self.click(*point)['command'],'')
        self.assertEqual(self.host.hover(690,110),'pointer')
        self.assertEqual(self.host.hover(900,110),'pointer')
        self.assertEqual(self.click(900,110)['command'],'')
        self.assertEqual(self.host.hover(150,520),'pointer');self.host.request('key 27')
        self.assertEqual(self.click(930,560)['command'],'@nav:settings')
        self.assertFalse(self.host.request('key 27')['closed'])
    def test_network_scan_completion_and_bounded_native_list_scroll(self):
        scanned=self.network_scan()
        self.assertEqual(self.host.hover(800,340),'pointer')
        frame=self.host.request('frame')
        self.assertEqual(self.network_crop(scanned,(640,245,1000,295)),self.network_crop(frame,(640,245,1000,295)),'periodic refresh must retain scan completion')
        below=self.host.request('wheel 800 410 600')
        self.assertNotEqual(self.network_crop(frame,(640,320,1000,500)),self.network_crop(below,(640,320,1000,500)))
        self.assertEqual(self.network_crop(frame,(20,220,620,500)),self.network_crop(below,(20,220,620,500)),'only the AP viewport scrolls')
        at_end=self.host.request('wheel 800 410 600')
        self.assertEqual(self.network_crop(below,(920,320,1000,500)),self.network_crop(at_end,(920,320,1000,500)),'RSSI positions remain fixed at the scroll boundary; long SSID text can marquee')
        self.host.request('wheel 800 410 -600');self.assertEqual(self.host.hover(800,340),'pointer')
    def test_network_password_back_escape_and_reentry_clear_text(self):
        self.network_scan();blank=self.click(800,340)
        self.assertEqual(self.host.hover(300,140),'text')
        self.assertEqual(self.host.hover(280,543),'pointer')
        for ch in 'sample-secret':self.host.request('key '+str(ord(ch)))
        cancelled=self.click(280,543);self.assertEqual(cancelled['command'],'')
        reopened=self.click(800,340)
        self.assertEqual(self.network_crop(blank,(60,115,950,170)),self.network_crop(reopened,(60,115,950,170)))
        self.assertFalse(self.host.request('key 27')['closed'])
        self.assertEqual(self.host.hover(800,340),'pointer')
        self.assertFalse(self.host.request('key 27')['closed'])
    def test_network_open_sample_connection_persists_between_pages(self):
        before=self.network_scan();self.click(800,450)
        connected=self.host.request('key 10')
        self.assertEqual(connected['command'],'')
        self.assertNotEqual(self.network_crop(before,(20,270,290,340)),self.network_crop(connected,(20,270,290,340)))
        self.assertEqual(self.host.hover(300,140),'default','successful Connect closes the keyboard')
        self.host.request('open tools-page 0');restored=self.host.request('open network-page 0')
        self.assertEqual(self.network_crop(connected,(20,270,290,340)),self.network_crop(restored,(20,270,290,340)))
    def test_network_credentials_commit_failure_and_retry(self):
        self.network_scan();self.click(800,395)
        for ch in 'local-only-password':self.host.request('key '+str(ord(ch)))
        self.host.request('frame',['settings-fail 1 0']);failed=self.host.request('key 10')
        self.assertEqual(failed['command'],'');self.assertEqual(self.host.hover(300,140),'text')
        self.host.request('frame',['settings-fail 0 0']);success=self.host.request('key 10')
        self.assertEqual(success['command'],'');self.assertEqual(self.host.hover(300,140),'default')
        self.assertNotEqual(self.network_crop(failed),self.network_crop(success))
    def test_network_configuration_and_connect_dispatch_failure_retry(self):
        for mode in [1,2]:
            self.network_scan(connect=mode);self.click(800,395)
            failed=self.host.request('key 10')
            self.assertEqual(failed['command'],'');self.assertEqual(self.host.hover(300,140),'text')
            self.host.request('frame',['network-fixture 0 0']);success=self.host.request('key 10')
            self.assertEqual(success['command'],'');self.assertEqual(self.host.hover(300,140),'default')
            self.assertNotEqual(self.network_crop(failed),self.network_crop(success))
    def test_network_auth_rejection_disables_controls_until_recovery(self):
        self.network_scan(connect=3);self.click(800,395);rejected=self.host.request('key 10')
        self.assertEqual(rejected['command'],'');self.assertEqual(self.host.hover(300,140),'default')
        self.host.request('open drybox-page 0')
        self.assertEqual(self.host.hover(120,478),'default');self.assertEqual(self.click(120,478)['command'],'')
        self.network_scan();self.click(800,395);self.host.request('key 10')
        self.host.request('open drybox-page 0');self.assertEqual(self.host.hover(120,478),'pointer')
        self.assertEqual(self.click(120,478)['command'],'DRY_PLA')
    def test_network_scan_error_empty_busy_and_timeout_release(self):
        for mode in [1,2,3,4,5,6]:
            failed=self.network_scan(mode=mode)
            self.assertFalse(failed['closed']);self.assertEqual(self.host.hover(800,340),'default')
            self.assertEqual(self.click(800,340)['command'],'')
            self.host.request('frame',['network-fixture 0 0']);self.click(690,110)
            self.assertEqual(self.host.hover(800,340),'pointer','Retry must release prior scan ownership')
    def test_network_repeated_password_teardown_and_page_recreation(self):
        for page in ['settings','tools','camera','dashboard','printer']*2:
            self.network_scan();self.click(800,340)
            self.assertFalse(self.host.request('open '+page+'-page 0')['closed'])
            self.assertTrue(self.host.request('close')['closed'])
        self.assertFalse(self.host.request('open network-page 0')['closed'])
    def profiles_manager(self):
        self.host.request('open network-page 0');return self.click(900,110)
    def profile_editor(self,row=120):
        self.profiles_manager();self.click(300,row);return self.click(600,520)
    def profile_field(self,point,old,new):
        self.click(*point)
        for _ in old:self.host.request('key 8')
        for ch in new:self.host.request('key '+str(ord(ch)))
        return self.click(750,540)
    def profile_status(self,result):return self.drybox_crop(result,(40,385,980,440))
    def test_profiles_chooser_selects_and_restores_separate_printer_state(self):
        self.host.request('open chooser-page 0')
        self.assertEqual(self.host.hover(512,220),'default')
        second=self.click(700,240);self.assertEqual(second['printer'],'Sermoon D1 #2')
        self.assertEqual(second['command'],'@nav:dashboard');self.assertEqual(second['job'],'');self.assertEqual(second['printState'],'standby');self.assertEqual(second['nozzle'],0)
        self.host.request('open chooser-page 0');first=self.click(300,240)
        self.assertEqual(first['printer'],'Sermoon D1');self.assertEqual(first['job'],'Spool holder.gcode');self.assertEqual(first['printState'],'printing');self.assertEqual(first['progress'],64)
    def test_profiles_manager_selection_close_and_empty_slot_feedback(self):
        self.profiles_manager();self.assertEqual(self.host.hover(500,405),'default')
        self.click(300,260)
        self.assertEqual(self.host.hover(860,520),'pointer');self.assertEqual(self.host.hover(400,520),'pointer')
        self.assertEqual(self.click(860,520)['printer'],'Sermoon D1')
        self.host.request('key 27');self.assertEqual(self.click(930,560)['command'],'@nav:settings')
    def test_profiles_rename_cancel_and_storage_failure_retry(self):
        self.profile_editor();self.profile_field((400,135),'Sermoon D1','Desk printer')
        cancelled=self.click(150,540);self.assertEqual(cancelled['printer'],'Sermoon D1')
        self.click(600,520);self.profile_field((400,135),'Sermoon D1','Desk printer')
        self.host.request('frame',['settings-fail 1 0']);failed=self.click(860,540)
        self.assertEqual(failed['printer'],'Sermoon D1');self.assertEqual(self.host.hover(400,135),'text')
        self.host.request('frame',['settings-fail 0 0']);saved=self.click(860,540)
        self.assertEqual(saved['printer'],'Desk printer');self.assertEqual(saved['job'],'Spool holder.gcode')
        self.assertEqual(self.host.hover(150,520),'pointer')
    def test_profiles_add_validates_host_port_and_uses_saved_draft(self):
        self.profile_editor(260);self.profile_field((400,135),'Printer 3','Fixture printer')
        self.click(860,540);self.assertEqual(self.host.hover(400,220),'text')
        self.profile_field((400,220),'','fixture.invalid');self.profile_field((900,220),'7125','65536')
        invalid=self.host.request('frame');self.assertEqual(invalid['printer'],'Sermoon D1');self.assertEqual(self.host.hover(400,110),'text','invalid port stays in the numeric editor')
        for _ in range(5):self.host.request('key 8')
        for ch in '7127':self.host.request('key '+str(ord(ch)))
        self.click(750,540);self.click(860,540)
        self.click(300,260);selected=self.click(860,520)
        self.assertEqual(selected['printer'],'Fixture printer');self.assertEqual(selected['printState'],'standby');self.assertEqual(selected['job'],'')
    def test_profiles_remove_confirmation_failed_commit_and_last_printer_guard(self):
        self.profiles_manager();self.click(300,190);self.click(400,520)
        self.click(300,424);self.assertEqual(self.host.hover(860,520),'pointer')
        self.click(400,520);self.host.request('frame',['settings-fail 1 0']);self.click(700,424)
        self.assertEqual(self.host.hover(700,424),'pointer')
        self.host.request('frame',['settings-fail 0 0']);self.click(700,424)
        retained=self.click(400,520);self.assertEqual(retained['printer'],'Sermoon D1','the last configured printer cannot be removed')
        self.assertEqual(self.host.hover(150,520),'pointer','guard feedback keeps the manager open')
        self.click(300,190);self.assertEqual(self.click(860,520)['printer'],'Sermoon D1','removed slot cannot be selected as active')
    def test_profiles_endpoint_test_success_failure_cancel_and_draft_change(self):
        self.profile_editor();initial=self.host.request('frame');self.click(630,540)
        verified=self.host.request('advance 300');self.assertNotEqual(self.profile_status(initial),self.profile_status(verified))
        self.host.request('frame',['profile-fixture 1 0']);self.click(630,540)
        failed=self.host.request('advance 300');self.assertNotEqual(self.profile_status(verified),self.profile_status(failed))
        self.host.request('frame',['profile-fixture 0 0']);self.click(630,540)
        self.profile_field((400,220),'simulation.invalid','changed.invalid')
        retired=self.host.request('advance 300');self.click(630,540);fresh=self.host.request('advance 300')
        self.assertNotEqual(self.profile_status(retired),self.profile_status(fresh))
        self.assertEqual(fresh['printer'],'Sermoon D1','connection tests never select or save a profile')
        self.host.request('key 27');self.click(600,520);self.click(630,540);self.host.request('advance 300')
    def test_profiles_discovery_empty_retry_and_reviewed_endpoint(self):
        self.profile_editor();self.host.request('frame',['profile-fixture 0 1']);self.click(150,282)
        self.host.request('advance 400');self.assertEqual(self.host.hover(400,170),'default')
        self.assertEqual(self.host.hover(300,530),'pointer')
        self.host.request('frame',['profile-fixture 0 0']);self.click(300,530);self.host.request('advance 400')
        self.assertEqual(self.host.hover(400,230),'pointer');draft=self.click(400,230)
        self.assertEqual(draft['printer'],'Sermoon D1');self.assertEqual(draft['job'],'Spool holder.gcode')
        self.assertEqual(self.host.hover(900,220),'text')
        self.click(150,540);self.assertEqual(self.host.hover(860,520),'pointer')
    def test_profiles_discovery_close_reopen_and_start_failure_retry(self):
        self.profile_editor();self.click(150,282);self.click(720,530)
        self.host.request('advance 400');self.assertEqual(self.host.hover(400,135),'text')
        self.host.request('frame',['profile-fixture 0 2']);self.click(150,282)
        self.assertEqual(self.host.hover(300,530),'pointer');self.host.request('frame',['profile-fixture 0 0'])
        self.click(300,530);self.host.request('advance 400');self.assertEqual(self.host.hover(400,170),'pointer')
        self.host.request('key 27');self.assertEqual(self.host.hover(400,135),'text')
    def test_profiles_api_key_draft_cancel_save_and_reopen(self):
        self.profile_editor();self.click(750,360)
        self.profile_field((400,250),'','sample-api-key');self.click(750,449)
        self.click(150,540);self.click(600,520);self.click(750,360)
        blank=self.host.request('frame');self.profile_field((400,250),'','sample-api-key')
        keyed=self.host.request('frame');self.assertNotEqual(self.drybox_crop(blank,(40,200,980,285)),self.drybox_crop(keyed,(40,200,980,285)))
        self.click(750,449);self.click(860,540);self.click(600,520);self.click(750,360)
        retained=self.host.request('frame');self.assertEqual(self.drybox_crop(keyed,(40,200,980,285)),self.drybox_crop(retained,(40,200,980,285)))
        self.assertEqual(retained['command'],'','sample keys never become frame commands')
    def test_profiles_security_synthetic_pem_and_port_draft(self):
        self.profile_editor();self.click(250,360);self.click(820,459)
        self.assertEqual(self.host.hover(300,165),'pointer')
        self.click(300,165);secure=self.host.request('frame')
        self.assertEqual(self.host.hover(900,220),'text')
        self.click(250,360);self.click(500,459)
        standard=self.host.request('frame');self.assertNotEqual(self.profile_status(secure),self.profile_status(standard))
        self.click(150,540);self.click(600,520);restored=self.host.request('frame')
        self.assertNotEqual(self.drybox_crop(secure,(850,195,980,250)),self.drybox_crop(restored,(850,195,980,250)),'Cancel discards the TLS-port draft')
    def test_profiles_camera_catalogs_are_isolated_between_printers(self):
        self.host.request('open camera-page 0');first=self.camera_pixels(self.camera_ready())
        self.host.request('open chooser-page 0');self.click(700,240)
        self.host.request('open camera-page 0');second=self.camera_pixels(self.camera_ready());self.assertNotEqual(first,second)
        self.click(390,460);self.click(680,215);self.host.request('key 27')
        self.host.request('open chooser-page 0');self.click(300,240)
        self.host.request('open camera-page 0');self.assertEqual(self.camera_pixels(self.camera_ready()),first)
    def test_profiles_repeated_nested_escape_and_teardown(self):
        for _ in range(3):
            self.profile_editor();self.click(250,360);self.click(820,459)
            self.host.request('key 27');self.host.request('key 27');self.click(750,360);self.click(400,250)
            for _ in range(4):self.assertFalse(self.host.request('key 27')['closed'])
            self.assertEqual(self.host.hover(900,110),'pointer')
            self.assertTrue(self.host.request('close')['closed'])
    def test_profiles_removing_active_printer_restores_remaining_context(self):
        self.host.request('open chooser-page 0');self.click(700,240)
        self.profiles_manager();self.click(400,520);removed=self.click(700,424)
        self.assertEqual(removed['printer'],'Sermoon D1');self.assertEqual(removed['job'],'Spool holder.gcode');self.assertEqual(removed['printState'],'printing')
    def test_profiles_switching_cannot_restore_online_state_without_wifi(self):
        self.host.request('open chooser-page 0');self.click(700,240);self.host.request('open chooser-page 0');self.click(300,240)
        self.network_scan(connect=3);self.click(800,395);self.host.request('key 10')
        for point in [(700,240),(300,240)]:
            self.host.request('open chooser-page 0');self.click(*point);self.host.request('open drybox-page 0')
            self.assertEqual(self.host.hover(120,478),'default');self.assertEqual(self.click(120,478)['command'],'')
    def telemetry_history(self):
        self.host.process.stdin.write(b'telemetry-info\n');self.host.process.stdin.flush();header=bytearray();deadline=time.monotonic()+5
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        parts=header.decode().split();self.assertEqual(parts[0],'HISTORY');return list(map(int,parts[1:]))
    def telemetry_crop(self,result,box=(36,261,493,357)):
        return Image.open(io.BytesIO(base64.b64decode(result['image'].split(',')[1]))).crop(box).tobytes()
    def test_telemetry_native_charts_tabs_scroll_and_inert_values(self):
        heat=self.host.request('open telemetry-page 0');self.assertFalse(heat['closed']);self.assertEqual(self.telemetry_history()[0],300)
        self.assertEqual(self.host.hover(200,280),'default');self.assertEqual(self.click(200,280)['command'],'')
        motion=self.click(175,160);dry=self.click(280,160)
        self.assertNotEqual(self.telemetry_crop(heat),self.telemetry_crop(motion));self.assertNotEqual(self.telemetry_crop(motion),self.telemetry_crop(dry))
        bottom=self.host.request('wheel 700 350 500');self.assertNotEqual(self.telemetry_crop(dry, (24,193,1000,485)),self.telemetry_crop(bottom,(24,193,1000,485)))
        top=self.host.request('wheel 700 350 -600');self.assertEqual(self.telemetry_crop(dry),self.telemetry_crop(top))
        self.assertEqual(self.click(200,104)['command'],'@tool:Devices')
    def test_telemetry_range_and_target_controls_use_real_callbacks(self):
        initial=self.host.request('open telemetry-page 0');self.click(400,160);short=self.click(385,202)
        self.assertNotEqual(self.telemetry_crop(initial),self.telemetry_crop(short))
        self.click(400,160);medium=self.click(385,247);self.assertNotEqual(self.telemetry_crop(short),self.telemetry_crop(medium))
        self.click(400,160);long=self.click(385,291);self.assertEqual(self.telemetry_crop(initial),self.telemetry_crop(long))
        detail=self.click(700,160);self.assertNotEqual(self.telemetry_crop(long),self.telemetry_crop(detail));restored=self.click(700,160);self.assertEqual(self.telemetry_crop(long),self.telemetry_crop(restored))
    def test_telemetry_hold_freezes_graph_but_live_values_and_history_continue(self):
        self.host.request('open telemetry-page 0');held=self.click(825,160)
        for point in [(175,160),(400,160),(550,160),(700,160)]:self.assertEqual(self.host.hover(*point),'default')
        changed=self.host.request('advance 2100',['telemetry-fixture 5']);self.assertEqual(self.telemetry_crop(held),self.telemetry_crop(changed));self.assertNotEqual(self.telemetry_crop(held,(400,200,494,230)),self.telemetry_crop(changed,(400,200,494,230)))
        self.assertEqual(self.telemetry_crop(changed),self.telemetry_crop(self.click(175,160)),'held tab is disabled')
        resumed=self.click(825,160);self.assertNotEqual(self.telemetry_crop(held),self.telemetry_crop(resumed));self.assertEqual(self.host.hover(175,160),'pointer')
    def test_telemetry_history_continues_hidden_and_offline_gaps_are_not_filled(self):
        self.host.request('open dashboard-page 0',['telemetry-fixture 6']);before=self.telemetry_history();self.assertEqual(before[0],1)
        self.host.request('advance 2100');live=self.telemetry_history();self.assertEqual(live[0],2)
        self.host.request('advance 6000',['telemetry-fixture 3']);offline=self.telemetry_history();self.assertEqual(offline,live)
        self.host.request('advance 2100',['telemetry-fixture 5']);reconnected=self.telemetry_history();self.assertEqual(reconnected[0],3);self.assertGreaterEqual(reconnected[3]-live[3],8100000)
        self.host.request('open telemetry-page 0');self.assertEqual(self.telemetry_history()[0],3)
    def test_telemetry_profile_switch_clears_history_and_releases_hold(self):
        self.host.request('open telemetry-page 0');old=self.telemetry_history();self.click(825,160)
        self.host.request('open chooser-page 0');self.click(700,240);self.host.request('open telemetry-page 0');new=self.telemetry_history()
        self.assertGreater(new[1],old[1]);self.assertLessEqual(new[0],2);self.assertEqual(self.host.hover(175,160),'pointer')
        self.host.request('open chooser-page 0');self.click(300,240);self.host.request('open telemetry-page 0');self.assertLessEqual(self.telemetry_history()[0],2,'switching back cannot merge histories')
    def test_telemetry_hotend_selector_and_missing_capabilities(self):
        self.host.request('open telemetry-page 0',['telemetry-fixture 1']);self.host.request('advance 2100');self.assertEqual(self.telemetry_history()[5],2)
        self.click(550,160);first=self.click(520,202);self.click(550,160);second=self.click(520,247)
        self.assertNotEqual(self.telemetry_crop(first,(400,200,494,230)),self.telemetry_crop(second,(400,200,494,230)))
        self.host.request('advance 2100',['telemetry-fixture 2']);self.assertEqual(self.telemetry_history()[4],31)
        self.assertEqual(self.click(200,280)['command'],'')
    def test_telemetry_repeated_teardown_preserves_shell_and_new_controls(self):
        for _ in range(5):
            self.host.request('open telemetry-page 0');self.click(400,160);self.host.request('close')
            self.host.request('open telemetry-page 0');self.assertEqual(self.host.hover(825,160),'pointer');self.click(825,160);self.host.request('key 27')
            self.assertEqual(self.click(90,560)['command'],'@nav:dashboard')
    def test_telemetry_empty_offline_waiting_and_reconnect_states(self):
        offline=self.host.request('open telemetry-page 0',['telemetry-fixture 6','telemetry-fixture 3']);self.assertEqual(self.telemetry_history()[0],0)
        waiting=self.host.request('frame',['telemetry-fixture 4']);self.assertEqual(self.telemetry_history()[0],0);self.assertNotEqual(self.telemetry_crop(offline,(760,85,1000,120)),self.telemetry_crop(waiting,(760,85,1000,120)))
        live=self.host.request('advance 2100',['telemetry-fixture 5']);self.assertEqual(self.telemetry_history()[0],1);self.assertNotEqual(self.telemetry_crop(waiting,(400,200,494,230)),self.telemetry_crop(live,(400,200,494,230)))
    def test_telemetry_dropdown_cleanup_estop_and_modal_scroll_isolation(self):
        self.host.request('open telemetry-page 0');self.click(400,160);self.host.request('key 27')
        self.assertEqual(self.host.hover(825,160),'pointer');self.click(915,35);self.assertEqual(self.host.hover(90,560),'default')
        before=self.host.request('frame');after=self.host.request('wheel 700 350 500');self.assertEqual(self.telemetry_crop(before),self.telemetry_crop(after))
        self.host.request('key 27');self.assertEqual(self.click(90,560)['command'],'@nav:dashboard')
    def device_info(self):
        self.host.process.stdin.write(b'devices-info\n');self.host.process.stdin.flush();deadline=time.monotonic()+5;header=bytearray()
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        parts=header.decode().split();self.assertEqual(parts[0],'DEVICES');return list(map(int,parts[1:]))
    def ui_text(self):
        self.host.process.stdin.write(b'ui-text\n');self.host.process.stdin.flush();deadline=time.monotonic()+5;header=bytearray()
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        size=int(header.decode().split()[1]);self.assertLess(size,32768);return self.host._read(size,deadline).decode()
    def devices_open(self,state='ready',setup=()):
        self.host.request('open devices-page 0',['state '+state,*setup]);return self.host.request('advance 600')
    def endstops_open(self,state='ready',mode=10):
        self.devices_open(state,['devices-fixture '+str(mode)]);return self.click(900,104)
    def test_devices_readonly_catalog_and_numeric_readings(self):
        self.devices_open();self.assertEqual(self.device_info()[0],24)
        text=self.ui_text();self.assertIn('60.0 / 60.0 C',text);self.assertIn('210.0 / 210.0 C',text);self.assertIn('65%',text)
        for point in [(350,158),(800,218),(500,400),(565,95)]:
            self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
        self.assertEqual(self.host.hover(735,104),'pointer');self.assertEqual(self.host.hover(900,104),'pointer')
        self.host.request('advance 600',['targets 220 65 40']);text=self.ui_text();self.assertIn('210.0 / 220.0 C',text);self.assertIn('60.0 / 65.0 C',text);self.assertIn('40%',text)
    def test_devices_filters_pagination_and_scroll_bounds(self):
        top=self.devices_open();self.assertEqual(self.host.hover(280,482),'default');self.assertEqual(self.host.hover(900,482),'pointer')
        bottom=self.host.request('wheel 700 300 500');self.assertNotEqual(self.telemetry_crop(top,(208,140,1000,450)),self.telemetry_crop(bottom,(208,140,1000,450)))
        self.host.request('wheel 700 300 -600');self.click(900,482);self.assertIn('Page 2 / 2',self.ui_text());self.assertEqual(self.host.hover(900,482),'default')
        for i,kind in enumerate(['Heat','Air','Power','Sensors','Outputs','Motion','Other']):
            self.click(100,210+i*44);text=self.ui_text();self.assertIn('Page 1 / 1',text);self.assertEqual(self.host.hover(900,482),'default',kind)
        self.click(100,166);self.assertIn('Page 1 / 2',self.ui_text())
    def test_devices_generic_values_update_without_rebuilding_catalog(self):
        self.devices_open();self.click(100,386);self.assertIn('100%',self.ui_text());before=self.device_info()
        self.host.request('advance 600',['devices-fixture 3']);after=self.device_info();self.assertEqual(before[2],after[2]);self.assertGreater(after[3],before[3]);self.assertIn('0%',self.ui_text())
        self.click(100,430);self.assertIn('0.90',self.ui_text())
    def test_devices_empty_and_truncated_catalogs(self):
        self.devices_open(setup=['devices-fixture 1']);self.assertEqual(self.device_info()[0],0);self.assertIn('No',self.ui_text());self.assertEqual(self.host.hover(900,482),'default')
        self.host.request('advance 600',['devices-fixture 2']);info=self.device_info();self.assertEqual(info[0],96);self.assertEqual(info[1],134);self.assertEqual(info[4],1);self.assertIn('96 of 134',self.ui_text())
    def test_devices_graphs_shell_and_profile_switching(self):
        self.devices_open();self.assertEqual(self.click(735,104)['command'],'@nav:telemetry')
        self.host.request('open telemetry-page 0');self.assertEqual(self.click(200,104)['command'],'@tool:Devices')
        self.host.request('open chooser-page 0');self.click(700,240);self.devices_open();self.assertEqual(self.device_info()[0],4);self.assertIn('Sermoon D1 #2',self.ui_text());self.assertIn('28.0 / 0.0 C',self.ui_text())
        self.assertEqual(self.click(90,560)['command'],'@nav:dashboard')
    def test_devices_offline_catalog_invalidates_and_reconnects(self):
        self.devices_open();self.host.request('advance 600',['telemetry-fixture 3']);self.assertEqual(self.device_info()[0],0);self.assertIn('Waiting for printer',self.ui_text())
        self.click(900,104);self.assertIn('Live readings unavailable',self.ui_text());self.assertEqual(self.device_info()[5],0)
        self.host.request('key 27');self.host.request('advance 600',['telemetry-fixture 5']);self.assertEqual(self.device_info()[0],24);self.assertIn('70.0 / 60.0 C',self.ui_text())
    def test_endstops_real_reply_rate_and_trigger_changes(self):
        self.endstops_open();self.assertEqual(self.device_info()[5],1);self.host.request('advance 300');text=self.ui_text();self.assertIn('x:  OPEN',text);self.assertIn('z:  TRIGGERED',text);self.assertEqual(self.device_info()[6:],[3,1,0])
        self.host.request('advance 600');self.assertEqual(self.device_info()[5],1)
        self.host.request('advance 2100',['devices-fixture 11']);self.host.request('advance 300');self.assertEqual(self.device_info()[5],2);self.assertIn('x:  TRIGGERED',self.ui_text())
    def test_endstops_printing_and_paused_do_not_query(self):
        for state in ['printing','paused']:
            self.endstops_open(state);self.host.request('advance 3000');self.assertEqual(self.device_info()[5],0);self.assertIn('Readings paused during a print',self.ui_text());self.host.request('key 27')
    def test_endstops_failure_retry_empty_and_timeout(self):
        self.endstops_open(mode=12);self.assertIn('Could not request endstops',self.ui_text());self.assertEqual(self.device_info()[5],1)
        self.host.request('advance 2200',['devices-fixture 10']);self.host.request('advance 300');self.assertIn('x:  OPEN',self.ui_text());self.assertEqual(self.device_info()[5],2)
        self.host.request('key 27');self.host.request('frame',['devices-fixture 14']);self.click(900,104);self.host.request('advance 300');self.assertIn('No endstops reported',self.ui_text())
        self.host.request('key 27');self.host.request('frame',['devices-fixture 15']);self.click(900,104);self.host.request('advance 5300');self.assertIn('timed out',self.ui_text());self.assertEqual(self.device_info()[8],0)
    def test_endstops_close_pending_and_modal_scroll_isolation(self):
        self.endstops_open();self.assertEqual(self.host.hover(90,560),'default');before=self.host.request('frame');after=self.host.request('wheel 700 300 500');self.assertEqual(self.telemetry_crop(before,(208,140,1000,450)),self.telemetry_crop(after,(208,140,1000,450)))
        self.host.request('key 27');self.host.request('advance 300');self.assertEqual(self.device_info()[6:],[0,0,0]);self.assertNotIn('Live endstops',self.ui_text())
        self.click(900,104);self.host.request('advance 300');self.assertIn('x:  OPEN',self.ui_text());self.click(760,490);self.assertNotIn('Live endstops',self.ui_text())
    def test_endstops_profile_change_discards_pending_reply(self):
        self.endstops_open();self.host.request('open chooser-page 0');self.click(700,240);self.devices_open();self.assertNotIn('Live endstops',self.ui_text());self.assertEqual(self.device_info()[6:],[0,0,0])
        self.click(900,104);self.host.request('advance 300');self.assertIn('x:  OPEN',self.ui_text())
    def macros_info(self):
        self.host.process.stdin.write(b'macros-info\n');self.host.process.stdin.flush();deadline=time.monotonic()+5;header=bytearray()
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        parts=header.decode().split();self.assertEqual(parts[0],'MACROS');return list(map(int,parts[1:]))
    def macros_open(self,setup=()):
        self.host.request('open macros-page 0',setup);return self.host.request('advance 600')
    def macro_text(self,text):
        for char in text:self.host.request('key '+str(ord(char)))
        return self.host.request('key 10')
    def macro_review(self,x=700,y=188):
        self.click(x,y);return self.click(700,510)
    def test_macros_native_catalog_private_filter_scroll_and_controls(self):
        top=self.macros_open();self.assertEqual(self.macros_info()[0],16);text=self.ui_text();self.assertIn('16 public macros',text);self.assertNotIn('_PRIVATE_HELPER',text)
        self.assertEqual(self.host.hover(200,138),'default');self.assertEqual(self.host.hover(900,104),'default');self.assertEqual(self.host.hover(700,188),'pointer')
        bottom=self.host.request('wheel 700 350 500');self.assertNotEqual(self.telemetry_crop(top,(24,160,1000,500)),self.telemetry_crop(bottom,(24,160,1000,500)))
        back=self.host.request('wheel 700 350 -600');self.assertEqual(self.telemetry_crop(top,(24,160,1000,500)),self.telemetry_crop(back,(24,160,1000,500)))
    def test_macros_search_done_cancel_no_matches_and_clear(self):
        self.macros_open();self.click(725,104);self.macro_text('clean');self.assertIn('1 match: clean',self.ui_text());self.assertEqual(self.host.hover(900,104),'pointer')
        self.click(725,104);self.host.request('key 8');self.host.request('key 27');self.assertIn('1 match: clean',self.ui_text())
        self.click(900,104);self.assertIn('16 public macros',self.ui_text());self.click(725,104);self.macro_text('no-such-macro');self.assertIn('No matching macros',self.ui_text());self.click(900,104)
        self.assertEqual(self.macros_info()[4:6],[0,0])
    def test_macros_parameter_keyboard_review_run_and_console(self):
        self.macros_open();self.click(700,188);self.assertIn('TEMP',self.ui_text());self.assertIn('SPEED',self.ui_text());self.click(450,190);self.macro_text('215')
        review=self.click(700,510);self.assertEqual(review['command'],'');self.assertIn('CLEAN_NOZZLE TEMP=215',self.ui_text());self.assertEqual(self.macros_info()[4],0)
        sent=self.click(700,510);self.assertEqual(sent['command'],'CLEAN_NOZZLE TEMP=215');self.assertEqual(self.macros_info()[4:6],[1,1]);self.assertNotIn('Run macro?',self.ui_text())
        self.host.request('open console-page 0');self.assertIn('CLEAN_NOZZLE TEMP=215',self.ui_text())
    def test_macros_cancel_and_nested_escape_never_dispatch(self):
        self.macros_open();self.click(700,188);self.click(450,190);self.host.request('key 50');self.host.request('key 27');self.assertNotIn('Edit parameter',self.ui_text());self.assertIn('CLEAN_NOZZLE',self.ui_text())
        self.click(300,510);self.assertNotIn('Review',self.ui_text());self.macro_review();self.click(300,510);self.assertNotIn('Run macro?',self.ui_text());self.assertEqual(self.macros_info()[4:6],[0,0])
    def test_macros_review_retained_for_rejected_send_and_retry(self):
        self.macros_open(['macros-fixture 3']);self.macro_review();self.assertEqual(self.click(700,510)['command'],'');self.assertIn('Macro not sent',self.ui_text());self.assertIn('Run macro?',self.ui_text());self.assertEqual(self.macros_info()[4:6],[1,0])
        self.host.request('frame',['macros-fixture 4']);self.assertEqual(self.click(700,510)['command'],'CLEAN_NOZZLE');self.assertEqual(self.macros_info()[4:6],[2,1])
    def test_macros_duplicate_review_blocked_then_allowed_after_interval(self):
        self.macros_open();self.macro_review();self.assertEqual(self.click(700,510)['command'],'CLEAN_NOZZLE');self.macro_review();self.assertEqual(self.host.hover(700,510),'pointer');self.assertEqual(self.click(700,510)['command'],'');self.assertIn('Macro already sent',self.ui_text());self.assertEqual(self.macros_info()[4:6],[1,1])
        self.host.request('advance 5100');self.assertEqual(self.host.hover(700,510),'pointer');self.assertEqual(self.click(700,510)['command'],'CLEAN_NOZZLE');self.assertEqual(self.macros_info()[4:6],[2,2])
    def test_macros_long_press_favorite_persists_in_local_nvs(self):
        self.macros_open();self.host.request('pointer 700 188 1');self.host.request('advance 650');self.host.request('pointer 700 188 0')
        self.assertEqual(self.macros_info()[6:],[1,1]);self.assertEqual(self.macros_info()[4:6],[0,0]);self.host.request('key 27');self.host.request('open tools-page 0');self.macros_open();self.assertEqual(self.macros_info()[6:],[1,1])
        self.click(230,188);self.assertIn('TEMP',self.ui_text());self.assertIn('SPEED',self.ui_text());self.assertEqual(self.macros_info()[4:6],[0,0])
    def test_macros_empty_truncation_offline_and_reconnect(self):
        self.macros_open(['macros-fixture 1']);self.assertIn('No public macros',self.ui_text());self.assertEqual(self.macros_info()[0],0)
        self.host.request('advance 600',['macros-fixture 2']);self.assertEqual(self.macros_info()[0:2],[64,86]);self.assertEqual(self.macros_info()[3],1)
        self.host.request('advance 600',['telemetry-fixture 3']);self.assertIn('Waiting for printer discovery',self.ui_text());self.assertEqual(self.macros_info()[0],0)
        self.host.request('advance 600',['telemetry-fixture 5','macros-fixture 0']);self.assertEqual(self.macros_info()[0],16)
    def test_macros_profile_change_retires_editor_and_pending_run(self):
        self.macros_open();self.macro_review();self.host.request('open chooser-page 0');self.click(700,240);self.macros_open();self.assertEqual(self.macros_info()[0],3);self.assertIn('Sermoon D1 #2',self.ui_text());self.assertNotIn('Run macro?',self.ui_text());self.assertEqual(self.macros_info()[4:6],[0,0])
        self.click(700,188);self.click(450,190);self.host.request('open dashboard-page 0');self.macros_open();self.assertNotIn('Edit parameter',self.ui_text());self.assertEqual(self.macros_info()[4:6],[0,0])
    def test_macros_offline_review_cannot_send_and_modal_isolation(self):
        self.macros_open();self.macro_review();self.assertEqual(self.host.hover(90,560),'default');self.host.request('advance 600',['telemetry-fixture 3']);self.assertEqual(self.host.hover(700,510),'default');self.assertEqual(self.click(700,510)['command'],'');self.assertEqual(self.macros_info()[4:6],[0,0])
        self.host.request('key 27');self.assertEqual(self.click(90,560)['command'],'@nav:dashboard')
    def test_macros_invalid_parameter_value_stays_in_editor_without_dispatch(self):
        self.macros_open();self.click(700,188);self.click(450,190);self.macro_text('210;M112');self.assertEqual(self.click(700,510)['command'],'');self.assertIn('Check parameters',self.ui_text());self.assertIn('semicolons',self.ui_text());self.assertNotIn('Run macro?',self.ui_text());self.assertEqual(self.macros_info()[4:6],[0,0])
        self.click(300,510);self.assertNotIn('Check parameters',self.ui_text())
    def test_macros_additional_parameter_space_quoting_and_blank_omission(self):
        self.macros_open();self.click(230,188);self.click(450,260);self.macro_text('NOTE');self.click(450,320);self.macro_text('warm up');self.click(700,510)
        self.assertIn('BED_HEAT_SOAK NOTE="warm up"',self.ui_text());self.assertNotIn('MINUTES=',self.ui_text());self.assertEqual(self.click(700,510)['command'],'BED_HEAT_SOAK NOTE="warm up"');self.assertEqual(self.macros_info()[4:6],[1,1])
    def mesh_info(self):
        self.host.process.stdin.write(b'mesh-info\n');self.host.process.stdin.flush();deadline=time.monotonic()+5;header=bytearray()
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        parts=header.decode().split();self.assertEqual(parts[0],'MESH');return list(map(int,parts[1:]))
    def mesh_open(self,state='ready',setup=()):
        return self.host.request('open mesh-page 0',['state '+state,*setup])
    def mesh_canvas(self,result):return self.telemetry_crop(result,(24,120,1000,418))
    def mesh_profiles(self):return self.click(285,476)
    def mesh_save_review(self,name):
        self.mesh_profiles();self.click(240,519)
        for char in name:self.host.request('key '+str(ord(char)))
        return self.click(700,539)
    def test_mesh_native_viewer_zoom_grid_reset_and_readonly_captions(self):
        baseline=self.mesh_open();self.assertEqual(self.mesh_info()[:5],[1,7,7,3,0]);self.assertIn('Profile default | 7 x 7',self.ui_text());self.assertIn('Min ',self.ui_text())
        for point in [(100,95),(200,432)]:self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
        self.assertEqual(self.host.hover(500,250),'grab');zoomed=self.click(618,476);self.assertNotEqual(self.mesh_canvas(zoomed),self.mesh_canvas(baseline));self.click(558,476);restored=self.click(730,476);self.assertEqual(self.mesh_canvas(restored),self.mesh_canvas(baseline))
        grid=self.click(900,476);self.assertNotEqual(self.mesh_canvas(grid),self.mesh_canvas(baseline));self.click(900,476);self.assertEqual(self.mesh_info()[5:],[0,0])
    def test_mesh_pointer_drag_rotates_and_cursor_tracks_capture(self):
        baseline=self.mesh_open();self.host.request('pointer 500 250 1');self.assertEqual(self.host.hover(500,250),'grabbing');self.host.request('pointer 560 280 1');rotated=self.host.request('pointer 600 300 0');self.assertNotEqual(self.mesh_canvas(baseline),self.mesh_canvas(rotated));self.assertEqual(self.host.hover(500,250),'grab');self.assertEqual(self.mesh_canvas(self.click(730,476)),self.mesh_canvas(baseline));self.assertEqual(self.mesh_info()[5:],[0,0])
    def test_mesh_load_profile_updates_real_controller_and_surface(self):
        baseline=self.mesh_open();self.mesh_profiles();self.assertEqual(self.host.hover(90,560),'default');self.assertEqual(self.host.hover(500,350),'default');self.click(350,290);loaded=self.click(420,519)
        self.assertEqual(loaded['command'],'BED_MESH_PROFILE LOAD=textured_plate');self.assertIn('Profile textured_plate',self.ui_text());self.assertNotEqual(self.mesh_canvas(baseline),self.mesh_canvas(loaded));self.assertEqual(self.mesh_info()[5:],[1,1]);self.assertNotIn('Bed mesh profiles',self.ui_text())
    def test_mesh_profile_name_guard_cancel_and_keyboard_escape(self):
        self.mesh_open();self.mesh_profiles();self.click(240,519);self.assertEqual(self.host.hover(700,539),'pointer');self.click(700,539);self.assertIn('Enter a name using letters',self.ui_text())
        for char in ' ;':self.host.request('key '+str(ord(char)))
        self.assertEqual(self.click(700,539)['command'],'');self.assertIn('Enter a name using letters',self.ui_text())
        for char in 'plate-1':self.host.request('key '+str(ord(char)))
        self.assertEqual(self.host.hover(700,539),'pointer');self.host.request('key 27');self.assertNotIn('Save bed mesh profile\n',self.ui_text());self.assertIn('Bed mesh profiles',self.ui_text());self.click(800,519);self.assertEqual(self.mesh_info()[5:],[0,0])
    def test_mesh_save_exact_reviewed_script_and_catalog_persistence(self):
        self.mesh_open();review=self.mesh_save_review('smooth_plate');self.assertEqual(review['command'],'');self.assertIn('Save bed mesh profile?',self.ui_text());self.assertIn('SAVE_CONFIG restarts Klipper',self.ui_text());self.assertEqual(self.mesh_info()[5:],[0,0])
        saved=self.click(700,464);self.assertEqual(saved['command'],'BED_MESH_PROFILE SAVE=smooth_plate\nSAVE_CONFIG');self.assertEqual(self.mesh_info()[3],4);self.assertEqual(self.mesh_info()[5:],[1,1]);self.assertIn('Profile smooth_plate',self.ui_text());self.mesh_profiles();self.assertIn('smooth_plate',self.ui_text());self.click(800,519)
        self.host.request('open console-page 0');self.assertIn('BED_MESH_PROFILE SAVE=smooth_plate',self.ui_text())
    def test_mesh_remove_back_then_confirm_retains_active_surface(self):
        baseline=self.mesh_open();self.mesh_profiles();self.click(350,236);self.click(600,519);self.assertIn('Remove bed mesh profile?',self.ui_text());self.assertEqual(self.mesh_info()[5:],[0,0]);self.click(350,464);self.assertIn('Bed mesh profiles',self.ui_text());self.click(600,519);removed=self.click(700,464)
        self.assertEqual(removed['command'],'BED_MESH_PROFILE REMOVE=petg_plate\nSAVE_CONFIG');self.assertEqual(self.mesh_info()[3],2);self.assertEqual(self.mesh_canvas(baseline),self.mesh_canvas(removed));self.mesh_profiles();self.assertNotIn('petg_plate',self.ui_text());self.click(800,519)
    def test_mesh_rejected_load_retains_selection_for_retry(self):
        self.mesh_open(setup=['mesh-fixture 3']);self.mesh_profiles();self.click(350,290);self.assertEqual(self.click(420,519)['command'],'');self.assertIn('Request was not accepted',self.ui_text());self.assertEqual(self.mesh_info()[5:],[1,0])
        self.host.request('frame',['mesh-fixture 4']);self.assertEqual(self.click(420,519)['command'],'BED_MESH_PROFILE LOAD=textured_plate');self.assertEqual(self.mesh_info()[5:],[2,1])
    def test_mesh_rejected_save_and_remove_retain_confirmation(self):
        self.mesh_open(setup=['mesh-fixture 3']);self.mesh_save_review('retry_plate');self.assertEqual(self.click(700,464)['command'],'');self.assertIn('Save bed mesh profile?',self.ui_text());self.assertIn('Request was not accepted',self.ui_text());self.host.request('frame',['mesh-fixture 4']);self.assertEqual(self.click(700,464)['command'],'BED_MESH_PROFILE SAVE=retry_plate\nSAVE_CONFIG')
        self.mesh_profiles();self.click(600,519);self.host.request('frame',['mesh-fixture 3']);self.assertEqual(self.click(700,464)['command'],'');self.assertIn('Remove bed mesh profile?',self.ui_text());self.host.request('frame',['mesh-fixture 4']);self.assertEqual(self.click(700,464)['command'],'BED_MESH_PROFILE REMOVE=retry_plate\nSAVE_CONFIG');self.assertEqual(self.mesh_info()[5:],[4,2])
    def test_mesh_calibration_back_escape_and_single_confirm_dispatch(self):
        baseline=self.mesh_open();self.click(450,476);self.assertIn('Calibrate bed mesh?',self.ui_text());self.assertEqual(self.host.hover(90,560),'default');self.click(350,429);self.assertEqual(self.mesh_info()[5:],[0,0])
        for _ in range(3):self.click(450,476);self.host.request('key 27');self.assertNotIn('Calibrate bed mesh?',self.ui_text())
        self.click(450,476);calibrated=self.click(650,429);self.assertEqual(calibrated['command'],'BED_MESH_CALIBRATE');self.assertEqual(self.mesh_info()[5:],[1,1]);self.assertNotEqual(self.mesh_canvas(baseline),self.mesh_canvas(calibrated));self.assertEqual(self.click(650,429)['command'],'');self.assertEqual(self.mesh_info()[5:],[1,1])
    def test_mesh_busy_offline_and_reconnect_action_guards(self):
        self.mesh_open('printing');self.assertEqual(self.host.hover(450,476),'default');self.mesh_profiles()
        for point in [(240,519),(420,519),(600,519)]:self.assertEqual(self.host.hover(*point),'default');self.assertEqual(self.click(*point)['command'],'')
        self.click(800,519);self.host.request('advance 300',['state paused']);self.assertEqual(self.host.hover(450,476),'default');self.host.request('advance 300',['state ready','telemetry-fixture 3']);self.assertEqual(self.mesh_info()[0],0);self.assertEqual(self.host.hover(450,476),'default')
        self.host.request('advance 300',['telemetry-fixture 5']);self.assertEqual(self.mesh_info()[0],1);self.assertEqual(self.host.hover(450,476),'pointer');self.assertEqual(self.mesh_info()[5:],[0,0])
    def test_mesh_profile_switches_keep_saved_samples_separate(self):
        self.mesh_open();self.mesh_save_review('cell_plate');self.click(700,464);self.host.request('open chooser-page 0');self.click(700,240);second=self.mesh_open();self.assertEqual(self.mesh_info()[3],1);self.assertNotIn('cell_plate',self.ui_text());self.assertIn('Sermoon D1 #2',self.ui_text())
        self.host.request('open chooser-page 0');self.click(300,240);first=self.mesh_open();self.assertEqual(self.mesh_info()[3],4);self.assertIn('Profile cell_plate',self.ui_text());self.assertNotEqual(self.mesh_canvas(first),self.mesh_canvas(second))
    def test_mesh_empty_and_bounded_large_matrix(self):
        self.mesh_open(setup=['mesh-fixture 1']);self.assertEqual(self.mesh_info()[:4],[0,0,0,0]);self.assertIn('No active bed mesh',self.ui_text());self.mesh_profiles();self.assertIn('No saved bed mesh profiles',self.ui_text());self.assertEqual(self.host.hover(240,519),'default');self.click(800,519)
        self.host.request('frame',['mesh-fixture 2']);self.assertEqual(self.mesh_info()[:5],[1,33,33,3,1]);self.assertIn('33 x 33',self.ui_text())
    def test_mesh_nested_estop_escape_and_page_teardown(self):
        for _ in range(3):
            self.mesh_open();self.click(450,476);self.host.request('key 27');self.mesh_profiles();self.click(240,519);self.host.request('key 27');self.host.request('key 27');self.assertEqual(self.host.hover(500,250),'grab')
            self.click(915,35);self.assertEqual(self.host.hover(500,250),'default');self.host.request('key 27');self.assertEqual(self.host.hover(500,250),'grab');self.mesh_profiles();self.host.request('open tools-page 0');self.mesh_open();self.assertNotIn('Bed mesh profiles',self.ui_text());self.assertEqual(self.mesh_info()[5:],[0,0])
    def calibration_info(self):
        self.host.process.stdin.write(b'calibration-info\n');self.host.process.stdin.flush();deadline=time.monotonic()+5;header=bytearray()
        while not header.endswith(b'\n'):header.extend(self.host._read(1,deadline))
        parts=header.decode().split();self.assertEqual(parts[0],'CALIBRATION');return list(map(int,parts[1:]))
    def calibration_open(self,state='ready',setup=()):
        self.host.request('open calibration-page 0',['state '+state,*setup]);return self.host.request('advance 600')
    def calibration_pid_start(self,mode=0):
        self.calibration_open(setup=['calibration-fixture '+str(mode)]);self.click(420,470);self.click(500,226);self.click(670,479);return self.click(670,464)
    def test_calibration_native_capabilities_bed_mesh_and_inert_captions(self):
        self.calibration_open();self.assertIn('13 tools detected',self.ui_text());self.assertEqual(self.calibration_info()[:2],[0,0]);self.assertEqual(self.host.hover(300,175),'default');self.assertEqual(self.host.hover(940,95),'default');self.assertEqual(self.click(125,230)['command'],'@tool:Bed mesh')
    def test_calibration_all_dialogs_escape_and_reopen_without_dispatch(self):
        self.calibration_open()
        workflows=[(400,230,'Run screws tilt?'),(125,280,'Run Z tilt?'),(400,280,'Start axis twist calibration?'),(605,230,'Motion limits'),(760,230,'Axis distance calculator'),(910,230,'TMC driver diagnostics'),(605,280,'Run input shaper calibration?'),(760,280,'Select resonance test axis'),(910,280,'Check accelerometer?'),(115,470,'Set up pressure advance tower'),(420,470,'Select heater for PID'),(620,420,'Start probe / Z calibration?'),(910,420,'Check probe accuracy?'),(620,470,'Custom calibration macros')]
        for _ in range(2):
            for x,y,title in workflows:
                self.click(x,y);self.assertIn(title,self.ui_text());self.assertEqual(self.host.hover(90,560),'default');self.host.request('key 27');self.assertNotIn(title,self.ui_text())
        self.assertEqual(self.calibration_info()[:2],[0,0])
    def test_calibration_screws_results_and_no_save(self):
        self.calibration_open();self.click(400,230);self.click(350,464);self.assertEqual(self.calibration_info()[:2],[0,0]);self.click(400,230);sent=self.click(670,464);self.assertEqual(sent['command'],'SCREWS_TILT_CALCULATE');self.host.request('advance 1200');self.assertIn('front_right: adjust CW 00:10',self.ui_text());self.assertIn('rear_right: adjust CCW 00:05',self.ui_text());self.assertEqual(self.calibration_info()[4:6],[1,0]);self.assertNotIn('Apply & restart',self.ui_text())
    def test_calibration_gantry_homes_first_and_blocks_duplicate(self):
        self.calibration_open(setup=['calibration-fixture 4']);self.click(125,280);self.assertIn('All axes will home first.',self.ui_text());self.assertEqual(self.click(670,464)['command'],'G28\nZ_TILT_ADJUST');self.assertEqual(self.click(670,464)['command'],'');self.assertEqual(self.calibration_info()[:2],[1,1])
    def test_calibration_input_shaper_results_then_reviewed_save_once(self):
        self.calibration_open();self.click(605,280);self.assertEqual(self.click(670,464)['command'],'SHAPER_CALIBRATE');self.assertEqual(self.host.hover(580,490),'default');self.host.request('advance 1200');self.assertIn('shaper_type_x=mzv',self.ui_text());self.assertEqual(self.calibration_info()[4:6],[1,1]);self.click(580,490);self.assertIn('Apply calibration & restart?',self.ui_text());self.click(350,444);self.assertEqual(self.calibration_info()[:2],[1,1]);self.click(580,490);saved=self.click(650,444);self.assertEqual(saved['command'],'SAVE_CONFIG');self.assertIn('Configuration save requested',self.ui_text());self.assertEqual(self.click(650,444)['command'],'');self.assertEqual(self.calibration_info()[:2],[2,2])
    def test_calibration_pid_target_selection_and_success(self):
        self.calibration_open();self.click(420,470);self.click(500,226);self.assertIn('Target: 200 C',self.ui_text());self.click(650,250);self.assertIn('Target: 210 C',self.ui_text());self.assertEqual(self.click(670,479)['command'],'');self.assertIn('Start PID heat cycle?',self.ui_text());self.assertEqual(self.click(670,464)['command'],'PID_CALIBRATE HEATER=extruder TARGET=210');self.host.request('advance 1200');self.assertIn('PID parameters',self.ui_text());self.assertEqual(self.calibration_info()[4:6],[1,1])
    def test_calibration_results_without_save_evidence_or_with_error(self):
        self.assertIn('PID_CALIBRATE',self.calibration_pid_start(7)['command']);self.host.request('advance 1200');self.assertEqual(self.calibration_info()[4:6],[0,0]);self.assertIn('PID parameters',self.ui_text());self.assertEqual(self.host.hover(580,490),'default')
        self.assertIn('PID_CALIBRATE',self.calibration_pid_start(3)['command']);self.host.request('advance 1200');self.assertEqual(self.calibration_info()[3],3);self.assertEqual(self.calibration_info()[5],0);self.assertIn('Simulated calibration failed',self.ui_text())
    def test_calibration_rejected_measurement_retains_review_for_retry(self):
        self.calibration_open(setup=['calibration-fixture 1']);self.click(605,280);self.assertEqual(self.click(670,464)['command'],'');self.assertIn('Run input shaper calibration?',self.ui_text());self.assertIn('not accept',self.ui_text());self.assertEqual(self.calibration_info()[:2],[1,0]);self.host.request('frame',['calibration-fixture 0']);self.assertEqual(self.click(670,464)['command'],'SHAPER_CALIBRATE');self.assertEqual(self.calibration_info()[:2],[2,1])
    def test_calibration_save_rejection_keeps_result_and_retry(self):
        self.calibration_pid_start();self.host.request('advance 1200');self.click(580,490);self.host.request('frame',['calibration-fixture 1']);self.assertEqual(self.click(650,444)['command'],'');self.assertIn('Your results are retained',self.ui_text());self.assertEqual(self.calibration_info()[:2],[2,1]);self.host.request('frame',['calibration-fixture 0']);self.assertEqual(self.click(650,444)['command'],'SAVE_CONFIG');self.assertEqual(self.calibration_info()[:2],[3,2])
    def test_calibration_probe_accuracy_statistics_never_enable_save(self):
        self.calibration_open();self.click(910,420);self.assertEqual(self.click(670,475)['command'],'PROBE_ACCURACY SAMPLES=10');self.host.request('advance 1200');self.assertIn('probe accuracy results:',self.ui_text());self.assertEqual(self.calibration_info()[2],9);self.assertEqual(self.calibration_info()[4:6],[1,0]);self.assertNotIn('Apply & restart',self.ui_text())
    def test_calibration_accuracy_requires_homing(self):
        self.calibration_open(setup=['calibration-fixture 4']);self.click(910,420);self.assertEqual(self.host.hover(670,475),'default');self.assertEqual(self.click(670,475)['command'],'');self.assertEqual(self.calibration_info()[:2],[0,0]);self.host.request('advance 600',['calibration-fixture 5']);self.assertEqual(self.host.hover(670,475),'pointer')
    def test_calibration_probe_manual_step_accept_and_abort(self):
        self.calibration_open(setup=['calibration-fixture 4']);self.click(620,420);self.assertEqual(self.click(670,464)['command'],'G28\nPROBE_CALIBRATE');before=self.host.request('frame')['position'][2];stepped=self.click(495,230);self.assertEqual(stepped['command'],'TESTZ Z=-0.05');self.assertAlmostEqual(stepped['position'][2],before-.05);self.assertEqual(self.click(670,500)['command'],'ACCEPT');self.host.request('advance 600');self.assertEqual(self.calibration_info()[4:6],[1,1]);self.assertIn('z_offset',self.ui_text())
        self.host.request('key 27');self.click(620,420);self.click(670,464);self.assertEqual(self.click(340,500)['command'],'ABORT');self.host.request('advance 1200');self.assertEqual(self.calibration_info()[4:6],[0,0])
    def test_calibration_axis_twist_three_samples_gate_save(self):
        self.calibration_open();self.click(400,280);self.assertEqual(self.click(670,464)['command'],'AXIS_TWIST_COMPENSATION_CALIBRATE')
        for i in range(3):
            self.assertEqual(self.click(670,500)['command'],'ACCEPT');self.assertEqual(self.calibration_info()[5],i==2)
        self.host.request('advance 600');self.assertIn('Apply & restart',self.ui_text());self.assertIn('axis twist calibration complete',self.ui_text())
    def test_calibration_motion_sensor_and_resonance_do_not_offer_save(self):
        self.calibration_open();self.click(910,280);self.assertEqual(self.click(670,464)['command'],'MEASURE_AXES_NOISE');self.host.request('advance 1200');self.assertIn('axes noise',self.ui_text());self.assertEqual(self.calibration_info()[4:6],[1,0]);self.assertNotIn('Apply & restart',self.ui_text());self.host.request('key 27');self.click(760,280);sent=self.click(350,280);self.assertEqual(sent['command'],'TEST_RESONANCES AXIS=X');self.host.request('advance 1200');self.assertIn('resonance data written',self.ui_text());self.assertEqual(self.calibration_info()[5],0)
    def test_calibration_pa_tower_only_changes_runtime_sample_limits(self):
        self.calibration_open();self.click(115,470);sent=self.click(320,290);self.assertEqual(sent['command'],'SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\nTUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=.005');self.assertIn('Pressure advance tower armed',self.ui_text());self.assertEqual(self.click(320,290)['command'],'');self.assertEqual(self.calibration_info()[:2],[1,1]);self.host.request('key 27');self.click(605,230);text=self.ui_text();self.assertIn('200.00 mm/s',text);self.assertIn('500.00 mm/s^2',text);self.assertIn('1.00 mm/s',text)
    def test_calibration_motion_values_driver_warnings_and_distance_calculator(self):
        self.calibration_open();self.click(605,230);self.assertIn('200.00 mm/s',self.ui_text());self.assertEqual(self.host.hover(780,180),'default');self.click(770,479);self.click(910,230);self.assertIn('0.65 A RMS',self.ui_text());self.assertIn('45.50 C',self.ui_text());self.host.request('advance 600',['calibration-fixture 6']);self.assertIn('Overtemperature warning',self.ui_text());self.host.request('key 27');self.click(760,230);self.click(700,510);self.assertIn('rotation_distance: 40.000000 mm',self.ui_text());self.click(300,254);self.host.request('key 8');self.macro_text('3');self.click(700,510);self.assertIn('rotation_distance: 60.000000 mm',self.ui_text());self.assertEqual(self.calibration_info()[:2],[0,0])
    def test_calibration_custom_macro_is_reviewed_before_local_send(self):
        self.calibration_open();self.click(620,470);self.assertIn('PID_BED',self.ui_text());self.assertEqual(self.calibration_info()[:2],[0,0]);self.click(500,170);self.assertIn('Run custom calibration?',self.ui_text());self.assertEqual(self.calibration_info()[:2],[0,0]);sent=self.click(670,464);self.assertEqual(sent['command'],'PID_BED');self.host.request('advance 1200');self.assertEqual(self.calibration_info()[4:6],[1,1])
    def test_calibration_busy_offline_and_limited_profile_guards(self):
        for state in ['printing','paused','shutdown']:
            self.calibration_open(state);self.assertEqual(self.host.hover(420,470),'default');self.assertEqual(self.click(420,470)['command'],'');self.assertEqual(self.calibration_info()[:2],[0,0])
        self.calibration_open(setup=['telemetry-fixture 3']);self.assertIn('Offline',self.ui_text());self.assertEqual(self.calibration_info()[:2],[0,0]);self.host.request('open chooser-page 0',['telemetry-fixture 5']);self.click(700,240);self.calibration_open();self.assertNotIn('Accuracy\n',self.ui_text());self.assertEqual(self.host.hover(910,420),'default')
    def test_calibration_pending_reply_cannot_complete_new_profile_session(self):
        self.calibration_pid_start();self.host.request('open chooser-page 0');self.click(700,240);self.calibration_open();self.host.request('advance 1200');self.assertEqual(self.calibration_info()[4:6],[0,0]);self.assertNotIn('PID parameters',self.ui_text());self.assertNotIn('Apply & restart',self.ui_text())
if __name__=='__main__':unittest.main(argv=[__file__,*remaining])
