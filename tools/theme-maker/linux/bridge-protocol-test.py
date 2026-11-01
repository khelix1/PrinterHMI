#!/usr/bin/env python3
"""Check macro-sized native command frames and reject lengths beyond the firmware bound."""
import io
import unittest
from types import SimpleNamespace
from native_preview import NativePreview

class ProtocolTests(unittest.TestCase):
    def bridge(self,command):
        raw=bytes(1024*600*4);job=b'sample.gcode';printer=b'Sermoon D1'
        header=f'FRAME 1 210 60 65 {len(command)} 1 2 3 0 100 100 printing 20 {len(job)} {len(printer)}\n'.encode()
        payload=io.BytesIO(header+command+job+printer+raw)
        bridge=NativePreview.__new__(NativePreview);bridge.process=SimpleNamespace(stdin=io.BytesIO());bridge._last_raw=None;bridge._last_image=None
        bridge._read=lambda count,deadline:payload.read(count)
        return bridge
    def test_maximum_macro_command_round_trip(self):
        command=b'PRINT_TEST '+b'x'*628
        result=self.bridge(command).request('frame');self.assertEqual(result['command'],command.decode());self.assertEqual(result['job'],'sample.gcode');self.assertEqual(result['printer'],'Sermoon D1');self.assertTrue(result['image'].startswith('data:image/png;base64,'))
    def test_utf8_length_is_counted_in_bytes(self):
        command=('PRINT_TEST '+('é'*300)).encode();self.assertEqual(self.bridge(command).request('frame')['command'],command.decode())
    def test_oversized_command_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError,'limits'):self.bridge(b'x'*640).request('frame')

if __name__=='__main__':unittest.main()
