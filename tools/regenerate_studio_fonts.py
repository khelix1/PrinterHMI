#!/usr/bin/env python3
"""Regenerate committed STUDIO fonts; requires fonttools and lv_font_conv 1.5.3."""
from pathlib import Path
import argparse
import subprocess
import tempfile
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--converter', default='lv_font_conv')
args = p.parse_args()
variants = [(18,400),(20,600),(24,600),(28,700),(32,700),(48,700),(64,700),(96,700)]
with tempfile.TemporaryDirectory(prefix='studio-fonts-') as folder:
    for weight in {w for _,w in variants}:
        face = instantiateVariableFont(TTFont(root/'main/assets/fonts/studio/Inter.ttf'),
            {'wght':weight,'opsz':24}, inplace=True)
        face.save(Path(folder)/f'inter-{weight}.ttf')
    for size,weight in variants:
        subprocess.run([args.converter,'--font',str(Path(folder)/f'inter-{weight}.ttf'),
            '--lv-include','lvgl.h','--size',str(size),'--bpp','4','--no-compress','--format','lvgl','--range','0x20-0x7e,0xb0',
            '--lv-font-name',f'ui_studio_font_{size}',
            '-o',str(root/f'main/assets/fonts/studio/inter_{size}.c')],check=True)

# Keep generator comments reproducible across machines.
for size,weight in variants:
    path=root/f'main/assets/fonts/studio/inter_{size}.c'
    lines=path.read_text().splitlines()
    lines=[f' * Source: Inter, weight {weight}; regenerate with tools/regenerate_studio_fonts.py'
        if line.startswith(' * Opts:') else line for line in lines]
    path.write_text('\n'.join(lines)+'\n')
