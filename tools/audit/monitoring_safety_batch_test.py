#!/usr/bin/env python3
"""Run Console, Event History and Telemetry lifecycle/layout audits as one batch."""
from pathlib import Path
import argparse,subprocess,sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl-dir',type=Path)
parser.add_argument('--lvgl-lib',type=Path)
args=parser.parse_args()
flags=[]
if args.lvgl_dir:flags+=['--lvgl-dir',str(args.lvgl_dir)]
if args.lvgl_lib:flags+=['--lvgl-lib',str(args.lvgl_lib)]
for name in ['console_modern_layout_test.py','event_history_layout_test.py','telemetry_modern_layout_test.py']:
    subprocess.run([sys.executable,str(Path(__file__).with_name(name)),*flags],check=True)
print('PASS: monitoring surface safety batch')
