#!/usr/bin/env python3
"""Run backup/network dialog, firmware dialog and OTA worker fault audits."""
from pathlib import Path
import argparse, subprocess, sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl-dir',type=Path)
parser.add_argument('--lvgl-lib',type=Path)
args=parser.parse_args()
flags=[]
if args.lvgl_dir:flags+=['--lvgl-dir',str(args.lvgl_dir)]
if args.lvgl_lib:flags+=['--lvgl-lib',str(args.lvgl_lib)]
for name,extra in [('network_backup_dialog_test.py',flags),
                   ('calibration_dialog_layout_test.py',[*flags,'--variant','OTAFAILURE']),
                   ('ota_transfer_test.py',[])]:
    subprocess.run([sys.executable,str(Path(__file__).with_name(name)),*extra],check=True)
print('PASS: backup, network and firmware update safety batch')
