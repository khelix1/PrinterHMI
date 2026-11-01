#!/usr/bin/env python3
"""Follow one sample print across real page widgets in both themes."""
import importlib.util
import sys
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('popup', Path(__file__).with_name('popup-parity-test.py'))
popup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(popup)


class DemoJourneyTests(popup.PopupParityTests):
    def journey(self, operator):
        if operator:
            self.operator()
        self.host.request('open files-page 0', ['state ready'])
        self.host.request('advance 25')
        self.press('Bracket.gcode')
        self.press('Start print' if operator else 'Print')
        started = self.press('Start print')
        self.assertEqual((started['command'], started['printState'], started['job']),
                         ('@nav:dashboard', 'printing', 'Bracket.gcode'))
        self.host.request('open dashboard-page 0')
        self.assertEqual(self.host.request('frame')['job'], 'Bracket.gcode')
        self.assertEqual(self.press('Pause')['printState'], 'paused')
        self.press('Resume')
        self.assertIn('Resume', self.text())
        self.assertEqual(self.press('Resume')['printState'], 'printing')
        self.host.request('key 27')
        self.host.request('open printer-page 0')
        self.assertEqual(self.host.request('frame')['job'], 'Bracket.gcode')
        self.press('Object')
        self.assertIn('part_1', self.text())
        self.press('Exclude')
        self.assertEqual(self.press('Exclude')['command'], 'EXCLUDE_OBJECT NAME=part_1')
        self.host.request('key 27')
        self.host.request('open dashboard-page 0')
        self.assertEqual(self.host.request('frame')['printState'], 'printing')

    def test_studio_journey(self):
        self.journey(False)

    def test_operator_journey(self):
        self.journey(True)


if __name__ == '__main__':
    suite = unittest.TestSuite(DemoJourneyTests(name) for name in ('test_studio_journey', 'test_operator_journey'))
    sys.exit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
