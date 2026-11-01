#!/usr/bin/env python3
"""Current firmware lighting and OTA flows through native pointer events."""
from pathlib import Path
import unittest
from native_preview import NativePreview
HELPER=Path(__file__).resolve().parents[3]/'build-theme-maker-native/printerhmi-ui-host'
class SimulationTests(unittest.TestCase):
    def setUp(self):self.host=NativePreview(HELPER)
    def tearDown(self):self.host.close()
    def click(self,x,y):
        self.host.request(f'pointer {x} {y} 1')
        return self.host.request(f'pointer {x} {y} 0')
    def text(self):
        self.host.process.stdin.write(b'ui-text\n');self.host.process.stdin.flush()
        header=self.host.process.stdout.readline().decode().split()
        import time
        return self.host._read(int(header[1]),time.monotonic()+5).decode()
    def press(self,name):
        self.host.process.stdin.write(('bounds '+name+'\n').encode());self.host.process.stdin.flush()
        found,x1,y1,x2,y2=map(int,self.host.process.stdout.readline().split()[1:])
        self.assertTrue(found,'Real button not found: '+name)
        return self.click((x1+x2)//2,(y1+y2)//2)
    def light(self,mode=0):
        self.host.request('open tools-page 0',[f'lighting-fixture {mode}'])
        self.press('Lighting')
    def test_lighting_switch_slider_and_close(self):
        self.light();self.assertIn('Live case lighting',self.text())
        self.assertEqual(self.click(300,210)['command'],'SET_PIN PIN=case_led VALUE=1')
        self.host.request('advance 300');self.assertIn('Brightness: 100%',self.text())
        self.host.request('pointer 510 293 1');self.assertEqual(self.host.request('frame')['command'],'')
        result=self.host.request('pointer 510 293 0');self.assertIn('SET_PIN PIN=case_led VALUE=0.',result['command'])
        self.click(700,465);self.assertEqual(self.host.hover(900,110),'pointer')
    def test_lighting_absent_and_profile_switch(self):
        self.host.request('open tools-page 0',['lighting-fixture 1']);self.assertEqual(self.host.hover(900,110),'default')
        self.assertNotIn('Lighting',self.text())
        self.host.request('open chooser-page 0',['lighting-fixture 0']);self.click(700,240)
        self.host.request('open tools-page 0');self.host.request('advance 300');self.assertNotIn('Lighting',self.text())
    def test_digital_slider_and_failed_send(self):
        self.light(2);self.assertEqual(self.host.hover(510,293),'default');self.assertEqual(self.click(300,210)['command'],'SET_PIN PIN=case_led VALUE=1')
        self.light(3);self.assertEqual(self.click(300,210)['command'],'');self.assertIn('Could not send',self.text())
        self.click(700,465);self.light(0);self.assertEqual(self.host.hover(510,293),'pointer')
    def test_pending_brightness_does_not_snap_back(self):
        self.light(4);self.host.request('pointer 510 293 1');self.host.request('pointer 510 293 0');self.assertIn('waiting for live confirmation',self.text())
        self.assertIn('Brightness: 49%',self.text());self.host.request('advance 1200');self.assertIn('Brightness: 49%',self.text());self.assertIn('Live case lighting',self.text())
    def test_ota_uses_real_channel_comparison(self):
        self.host.request('open ota 0');self.assertIn('Installed: Development | '+(Path(__file__).resolve().parents[3]/'version.txt').read_text().strip(),self.text());self.assertIn('Switch to Stable',self.text())
        self.click(500,240);self.assertIn('Switch to Stable v7.2.0',self.text());self.assertNotIn('Downgrade',self.text());self.click(350,485)
        self.click(700,140);self.assertIn('nightly-2026-10-30',self.text());self.click(500,240);self.assertIn('Install Nightly',self.text())
    def test_ota_catalog_failure_and_retry(self):
        self.host.request('open ota 0',['ota-fixture 1']);self.assertIn('unavailable',self.text());self.host.request('frame',['ota-fixture 0']);self.click(500,530);self.host.request('advance 300');self.assertIn('Switch to Stable',self.text())
    def test_ota_simulated_transfer_and_cancellation(self):
        self.host.request('open ota 0');self.click(500,240);self.click(700,485);self.host.request('advance 300');self.assertIn('Simulating firmware transfer',self.text())
        # Actual progress Cancel dispatches to the local timer adapter.
        self.click(510,480);self.host.request('advance 3000');self.assertIn('No firmware was changed',self.text())
    def test_setup_welcome_continue_checkmarks_and_review(self):
        self.host.request('open setup 0');self.assertIn('Welcome to your print cell',self.text())
        self.press('Continue');self.assertIn('Your network',self.text());self.assertIn(' Welcome',self.text())
        self.press('Time zone');self.assertIn('Set your time zone',self.text());self.press('Continue');self.assertIn(' Time zone',self.text());self.assertIn('Display & appearance',self.text())
        self.press('Continue');self.assertIn(' Display',self.text());self.press('Review');self.assertIn('Review your setup',self.text());self.press('Finish setup');self.assertIn('Setup complete',self.text());self.press('Done');self.assertNotIn('Setup complete',self.text())
    def test_setup_camera_discovery_test_save(self):
        self.host.request('open setup 0');self.press('Camera');self.press('Discover and test');self.host.request('advance 300');self.assertIn('Select a camera',self.text());self.press('Front camera');self.host.request('advance 300');self.assertIn('Camera verified',self.text());self.press('Use camera');self.assertIn('Your camera',self.text())
    def test_setup_network_scan_connect_and_verification(self):
        self.host.request('open setup 0');self.press('Network');self.press('Change network');self.host.request('advance 300');self.assertIn('Workshop',self.text())
        # AP choices include RSSI in their actual firmware labels.
        text=self.text();choice=next(line for line in text.splitlines() if line.startswith('Workshop  '))
        self.press(choice)
        for char in 'sample-password':self.host.request('key '+str(ord(char)))
        self.press('Connect');self.host.request('advance 300');self.host.request('advance 800');self.assertIn('Workshop',self.text());self.assertIn('Your network',self.text())
    def test_setup_profile_editor_and_discovery_use_shared_dialogs(self):
        self.host.request('open setup 0');self.press('Printer');self.press('Edit printer');self.assertIn('Moonraker',self.text());self.assertNotIn('Printer profiles are simulated',self.text());self.host.request('key 27');self.assertIn('Your printer',self.text())
    def test_setup_close_reopen_and_pending_scan_teardown(self):
        for _ in range(3):
            self.host.request('open setup 0');self.press('Camera');self.press('Discover and test');self.host.request('close');self.host.request('advance 500');self.host.request('open setup 0');self.assertIn('Welcome to your print cell',self.text());self.press('Set up later');self.assertNotIn('Welcome to your print cell',self.text())
    def test_scenario_progress_pause_resume_completion_and_offline(self):
        self.host.request('open dashboard-page 0');before=self.host.request('scenario printing');advanced=self.host.request('advance 10000');self.assertGreater(advanced['progress'],before['progress']);self.host.request('scenario paused');paused=self.host.request('advance 10000');self.assertAlmostEqual(paused['progress'],35,places=1)
        self.host.request('open printer-page 0');self.assertEqual(self.host.request('frame')['printState'],'paused')
        self.host.request('scenario complete');self.assertEqual(self.host.request('frame')['progress'],100);self.host.request('scenario offline');self.assertEqual(self.host.request('frame')['printState'],'offline');self.host.request('scenario idle');self.assertEqual(self.host.request('frame')['job'],'')
    def test_simulation_clock_can_freeze_without_rewinding(self):
        self.host.request('open dashboard-page 0');self.host.request('scenario printing');self.host.request('simulation-run 0');before=self.host.request('frame')['progress'];after=self.host.request('advance 10000')['progress'];self.assertEqual(before,after)
    def test_setup_failed_persistence_retries_without_false_completion(self):
        self.host.request('open setup 0',['settings-fail 1 0']);self.press('Continue');self.assertIn('Could not save step completion',self.text());self.assertIn('Welcome to your print cell',self.text());self.host.request('frame',['settings-fail 0 0']);self.press('Continue');self.assertIn(' Welcome',self.text())
    def test_setup_appearance_rebuild_and_native_operator_pages(self):
        self.host.request('open setup 0');self.press('Display');self.press('Operator');self.host.request('advance 300');self.assertIn('Display & appearance',self.text());self.press('Set up later');self.host.request('open tools-page 0');self.assertIn('Lighting',self.text());self.light();self.assertIn('Live case lighting',self.text())
if __name__=='__main__':unittest.main()
