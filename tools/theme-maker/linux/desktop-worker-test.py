#!/usr/bin/env python3
"""Check desktop bridge threading and FIFO delivery without a GTK installation."""
import ast
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
from threading import Event, current_thread, main_thread
from types import SimpleNamespace
import unittest

source=ast.parse(Path(__file__).with_name('theme-maker.py').read_text())
original=next(n for n in source.body if isinstance(n,ast.ClassDef) and n.name=='ThemeMaker')
selected=ast.ClassDef(name='Shell',bases=[],keywords=[],body=[n for n in original.body if isinstance(n,ast.FunctionDef) and n.name in ('message','native_message','callback')],decorator_list=[])
module=ast.fix_missing_locations(ast.Module(body=[selected],type_ignores=[]))

class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.idle=[];self.scripts=[];self.calls=[];self.entered=Event();self.release=Event();test=self
        class Preview:
            def __init__(self,path):test.calls.append('create')
            def request(self,line,setup=()):
                test.assertIsNot(current_thread(),main_thread());test.calls.append(line)
                if line=='frame':test.entered.set();test.release.wait(2)
                return {'image':'sample','closed':False}
            def close(self):test.calls.append('retire')
        env={'NativePreview':Preview,'current_thread':current_thread,'main_thread':main_thread,'GLib':SimpleNamespace(idle_add=lambda fn:self.idle.append(fn)), 'json':json,'re':re,'MAX_TEXT':10000,'ENTRY':'file:///editor'}
        exec(compile(module,'desktop-worker-fixture','exec'),env)
        self.shell=env['Shell']();self.shell.native=None;self.shell.export_busy=False
        self.shell.native_worker=ThreadPoolExecutor(max_workers=1)
        self.shell.native_executable=lambda:Path('/fixture/helper')
        def evaluate(*args):
            self.assertIs(current_thread(),main_thread());self.scripts.append(args[0])
        self.shell.view=SimpleNamespace(get_uri=lambda:'file:///editor',evaluate_javascript=evaluate)
    def tearDown(self):
        self.release.set();self.shell.native_worker.shutdown(wait=True)
    def send(self,operation,**extra):
        raw=json.dumps({'action':'native-ui','operation':operation,'token':1,**extra})
        self.shell.message(None,SimpleNamespace(get_js_value=lambda:SimpleNamespace(to_string=lambda:raw)))
    def drain(self):
        self.release.set();self.shell.native_worker.submit(lambda:None).result(timeout=3)
        for fn in self.idle:fn()
        self.idle.clear()
    def test_frame_does_not_block_desktop_and_callbacks_use_main_thread(self):
        self.send('frame');self.assertTrue(self.entered.wait(1));self.assertFalse(self.release.is_set());self.assertFalse(self.scripts)
        self.send('pointer',x=25,y=50,down=1);self.send('pointer',x=25,y=50,down=0)
        self.drain();self.assertEqual(self.calls,['create','frame','pointer 25 50 1','pointer 25 50 0']);self.assertEqual(len(self.scripts),3)
    def test_native_refresh_during_file_dialog_preserves_export_guard(self):
        self.shell.export_busy=True;self.send('frame');self.drain()
        self.assertTrue(self.shell.export_busy);self.assertEqual(len(self.scripts),1)
    def test_reset_keeps_fifo_and_recreates_helper(self):
        self.send('frame');self.assertTrue(self.entered.wait(1));self.send('reset');self.send('frame');self.drain()
        self.assertEqual(self.calls,['create','frame','retire','create','frame'])
    def test_validation_error_retires_helper_and_next_request_recovers(self):
        self.send('wheel',x=1,y=2,delta='invalid');self.send('frame');self.drain()
        self.assertEqual(self.calls,['create','retire','create','frame']);self.assertIn('ThemeMakerNativeError',self.scripts[0]);self.assertIn('ThemeMakerNativeFrame',self.scripts[1])

if __name__=='__main__':unittest.main()
