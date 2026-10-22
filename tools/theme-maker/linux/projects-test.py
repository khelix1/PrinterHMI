#!/usr/bin/env python3
"""Editable project portability, archive boundaries and native dialog routes."""
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile
from PIL import Image
from artwork import cached_source, store_source
from projects import document, pack, unpack, save_request

BASE = Path(__file__).resolve().parents[1]


def archive(entries):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in entries:
            z.writestr(name, data)
    return stream.getvalue()


class ProjectsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.p = json.loads(subprocess.check_output(['node', '-e', "console.log(require('./project-model').serialize(require('./theme-model').create('studio')))"], cwd=BASE, text=True))

    def tearDown(self): self.temp.cleanup()

    def test_portability_with_and_without_artwork(self):
        for artwork in (False, True):
            if artwork:
                stream = io.BytesIO(); Image.new('RGB', (80, 60), '#6420AD').save(stream, 'PNG')
                asset, _ = store_source(stream.getvalue(), self.root/'old-computer')
                self.p['theme'].update(schema_version=3, background={'asset': asset, 'fit':'cover', 'position_x':30, 'position_y':75, 'overlay_color':'#121212', 'overlay_opacity':40})
            text = json.dumps(self.p); data = pack(text, self.root/'old-computer')
            opened = unpack(data, self.root/'new-computer'); self.assertEqual(opened, self.p)
            if artwork:
                self.assertEqual(cached_source(self.root/'old-computer',asset), cached_source(self.root/'new-computer',asset))
            subprocess.run(['node', '-e', "require('./project-model').parse(process.argv[1]);", json.dumps(opened)], cwd=BASE, check=True)

    def test_reject_bad_archives_and_metadata(self):
        text=json.dumps(self.p)
        cases=[b'not a zip',archive([('../project.json',text)]),archive([('project.json',text),('extra','data')]),archive([('project.json','x'*65537)]),archive([('project.json',text),('source.png',b'bad')])]
        for data in cases:
            with self.assertRaises((ValueError,zipfile.BadZipFile)):
                unpack(data,self.root/'cache')
        self.assertFalse((self.root/'cache').exists())
        for change in [dict(version=2),dict(editor={}),dict(theme={}),dict(extra=True)]:
            p=dict(self.p,**change)
            with self.assertRaises(ValueError): document(json.dumps(p))
        with self.assertRaises(ValueError): document(text.replace('"version": 1','"version": 1, "version": 1'))
        for name in ['../outside.phmiproject','/tmp/x.phmiproject','theme.py']:
            with self.assertRaises(ValueError):save_request(json.dumps({'action':'save-project','filename':name,'text':text}))

    def test_mismatched_or_missing_artwork(self):
        self.p['theme'].update(schema_version=3,background={'asset':'bg-'+'a'*64+'.phmibg','fit':'cover','position_x':50,'position_y':50,'overlay_color':'#000000','overlay_opacity':30})
        with self.assertRaises(ValueError):pack(json.dumps(self.p),self.root)
        for entries in [[('project.json',json.dumps(self.p))],[('project.json',json.dumps(self.p)),('source.png',b'wrong')]]:
            with self.assertRaises(ValueError):unpack(archive(entries),self.root/'cache')
        self.assertFalse((self.root/'cache').exists())

    def test_native_save_open_and_cancel_routes(self):
        # Exercise the actual message dispatcher without installing or opening GTK.
        gtk=types.SimpleNamespace(Application=object,FileChooserAction=types.SimpleNamespace(OPEN=1,SAVE=2))
        gi=types.ModuleType('gi');gi.require_version=lambda *a:None
        repository=types.ModuleType('gi.repository');repository.Gtk=gtk
        repository.Gio=repository.GLib=repository.WebKit2=types.SimpleNamespace()
        with patch.dict(sys.modules,{'gi':gi,'gi.repository':repository}):
            spec=importlib.util.spec_from_file_location('project_native_fixture',BASE/'linux/theme-maker.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        app=module.ThemeMaker.__new__(module.ThemeMaker);app.export_busy=False;app.artwork_dir=self.root/'cache'
        app.view=types.SimpleNamespace(get_uri=lambda:module.ENTRY)
        notices=[];callbacks=[];app.notify=lambda *args:notices.append(args);app.callback=lambda *args:callbacks.append(args)
        target=self.root/'my-project.phmiproject';app.choose_file=lambda *args:str(target)
        def send(request):
            result=types.SimpleNamespace(get_js_value=lambda:types.SimpleNamespace(to_string=lambda:json.dumps(request)))
            app.message(None,result);self.assertFalse(app.export_busy)
        send({'action':'save-project','filename':'my-project.phmiproject','text':json.dumps(self.p)})
        self.assertEqual(unpack(target.read_bytes(),self.root/'other'),self.p);self.assertIn('Project saved',notices[-1][0])
        send({'action':'open-project'});self.assertEqual(callbacks[-1][0],'ThemeMakerWorkspaceLoaded');self.assertEqual(callbacks[-1][1]['document'],self.p)
        before=target.read_bytes();app.choose_file=lambda *args:None
        send({'action':'save-project','filename':'my-project.phmiproject','text':json.dumps(self.p)});self.assertIn('cancelled',notices[-1][0]);self.assertEqual(before,target.read_bytes())
        count=len(callbacks);send({'action':'open-project'});self.assertEqual(len(callbacks),count);self.assertIn('cancelled',notices[-1][0])
        app.choose_file=lambda *args:str(target);target.write_bytes(b'broken');send({'action':'open-project'});self.assertEqual(len(callbacks),count);self.assertTrue(notices[-1][1])


if __name__=='__main__':unittest.main()
