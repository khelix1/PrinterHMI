#!/usr/bin/env python3
"""Exercise native export trust boundaries and failure-safe writes without GTK."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from desktop_io import atomic_save, export_request


class NativeExportTests(unittest.TestCase):
    def request(self, **kwargs):
        value = {'action': 'export', 'filename': 'my-theme.phmitheme',
                 'text': json.dumps({'schema': 'printerhmi.theme', 'name': 'Ceramic'})}
        value.update(kwargs)
        return json.dumps(value)

    def test_export_contract(self):
        name, text = export_request(self.request())
        self.assertEqual(name, 'my-theme.phmitheme')
        self.assertEqual(json.loads(text)['name'], 'Ceramic')
        for filename in ['../theme.phmitheme', '/tmp/theme.phmitheme', 'x.phmitheme', 'theme.sh', None]:
            with self.assertRaises(ValueError):
                export_request(self.request(filename=filename))
        for action in ['delete', 'open', None]:
            with self.assertRaises(ValueError):
                export_request(self.request(action=action))
        for text in ['x' * 49153, '{}', '[]', '{broken', '', None]:
            with self.assertRaises((ValueError, TypeError)):
                export_request(self.request(text=text))
        with self.assertRaises(ValueError):
            export_request('x' * 100000)

    def test_atomic_export_and_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            dest = Path(folder) / 'my-theme.phmitheme'
            dest.write_text('previous theme')
            with patch('desktop_io.os.replace', side_effect=OSError('disk error')):
                with self.assertRaises(OSError):
                    atomic_save(dest, 'new theme')
            self.assertEqual(dest.read_text(), 'previous theme')
            self.assertEqual(list(Path(folder).iterdir()), [dest])
            atomic_save(dest, 'Ceramic · café')
            self.assertEqual(dest.read_text(), 'Ceramic · café')
            self.assertEqual(list(Path(folder).iterdir()), [dest])


if __name__ == '__main__':
    unittest.main()
