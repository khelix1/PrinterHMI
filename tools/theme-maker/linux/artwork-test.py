#!/usr/bin/env python3
"""Bounded image composition and safe, editable ZIP round-trips."""
import copy
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile
from PIL import Image
from artwork import (HEADER, PIXEL_BYTES, bundle, cached_source, compose, encode_asset,
                     import_bundle, source_image, store_source, validate_asset)

BASE = Path(__file__).resolve().parents[1]


class ArtworkTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(); self.directory = Path(self.folder.name)
        image = Image.new('RGB', (200, 100), '#FF0000'); image.paste('#0000FF', (100, 0, 200, 100))
        stream = io.BytesIO(); image.save(stream, 'PNG'); self.source = stream.getvalue()
        name, _ = store_source(self.source, self.directory)
        self.bg = {'asset': name, 'fit': 'contain', 'position_x': 50, 'position_y': 50,
                   'overlay_color': '#000000', 'overlay_opacity': 0}
        self.project = json.loads(subprocess.check_output(['node', '-e', "console.log(JSON.stringify(require('./theme-model.js').createBlank()))"], cwd=BASE, text=True))
        self.project.update(schema_version=3, background=self.bg)

    def tearDown(self): self.folder.cleanup()

    def test_crop_placement_overlay_and_raw_contract(self):
        png = cached_source(self.directory, self.bg['asset'])
        centered = compose(png, self.bg, '#FFFFFF')
        self.assertEqual(centered.getpixel((10, 0)), (255, 255, 255))
        self.assertEqual(centered.getpixel((10, 100)), (255, 0, 0))
        self.assertEqual(centered.getpixel((900, 100)), (0, 0, 255))
        top = compose(png, dict(self.bg, position_y=0), '#FFFFFF')
        self.assertEqual(top.getpixel((10, 0)), (255, 0, 0))
        cover = compose(png, dict(self.bg, fit='cover'), '#FFFFFF')
        self.assertEqual(cover.getpixel((10, 0)), (255, 0, 0))
        dark = compose(png, dict(self.bg, fit='stretch', overlay_opacity=100), '#FFFFFF')
        self.assertEqual(dark.getpixel((900, 100)), (0, 0, 0))
        asset = encode_asset(centered); validate_asset(asset)
        self.assertEqual(len(asset), HEADER.size + PIXEL_BYTES)
        damaged = bytearray(asset); damaged[-1] ^= 1
        with self.assertRaises(ValueError): validate_asset(damaged)
        with self.assertRaises(ValueError): validate_asset(asset[:-1])

    def test_jpeg_orientation_and_panorama_limits(self):
        image = Image.new('RGB', (200, 100), '#FF0000'); stream = io.BytesIO()
        exif = Image.Exif(); exif[274] = 6; image.save(stream, 'JPEG', exif=exif)
        _, size = source_image(stream.getvalue()); self.assertEqual(size, (100, 200))
        stream = io.BytesIO(); Image.new('RGB', (1600, 1)).save(stream, 'PNG')
        with self.assertRaises(ValueError): compose(stream.getvalue(), dict(self.bg, fit='cover'), '#FFFFFF')
        self.assertEqual(compose(stream.getvalue(), self.bg, '#FFFFFF').size, (1024, 600))

    def test_editable_bundle_round_trip(self):
        packed = bundle(json.dumps(self.project), self.directory)
        result = import_bundle(packed, self.directory)
        self.assertEqual(result, self.project)
        with zipfile.ZipFile(io.BytesIO(packed)) as archive:
            theme = json.loads(archive.read('PrinterHMI/themes/untitled-theme.phmitheme'))
            data = archive.read('PrinterHMI/themes/' + theme['background']['asset']); validate_asset(data)
            self.assertEqual(archive.read('ThemeMaker/source.png'), cached_source(self.directory, self.bg['asset']))
        # Native output must also satisfy the portable editor model.
        subprocess.run(['node', '-e', "const M=require('./theme-model.js');M.parse(process.argv[1]);", json.dumps(result)], cwd=BASE, check=True)

    def test_reject_path_bombs_and_unsupported_images(self):
        bad = dict(self.bg, asset='../outside.phmibg')
        with self.assertRaises(ValueError): compose(self.source, bad, '#FFFFFF')
        with self.assertRaises((ValueError, OSError)): source_image(b'not an image')
        with self.assertRaises(ValueError): source_image(b'x' * (12 * 1024 * 1024 + 1))
        tiny = Image.new('RGB', (1, 1)); stream = io.BytesIO(); tiny.save(stream, 'GIF')
        with self.assertRaises(ValueError): source_image(stream.getvalue())
        raw = bundle(json.dumps(self.project), self.directory)
        for evil in ['../theme.phmitheme', '/tmp/outside', 'unexpected.txt']:
            stream = io.BytesIO(raw)
            with zipfile.ZipFile(stream, 'a') as archive: archive.writestr(evil, 'bad')
            with self.assertRaises(ValueError): import_bundle(stream.getvalue(), self.directory)
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('ThemeMaker/source.png', b'0' * (5 * 1024 * 1024))
            archive.writestr('INSTALL.txt', '')
            archive.writestr('PrinterHMI/themes/x.phmitheme', '{}')
        with self.assertRaises(ValueError): import_bundle(stream.getvalue(), self.directory)
        (self.directory / (self.bg['asset'] + '.png')).write_bytes(b'damaged')
        with self.assertRaises(ValueError): cached_source(self.directory, self.bg['asset'])
        repaired, _ = store_source(self.source, self.directory)
        self.assertEqual(repaired, self.bg['asset']); self.assertTrue(cached_source(self.directory, repaired))


if __name__ == '__main__': unittest.main()
