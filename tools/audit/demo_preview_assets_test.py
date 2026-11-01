#!/usr/bin/env python3
"""Decode every embedded PNG and verify its RGB565 preview comes from the same artwork."""
from pathlib import Path
from PIL import Image
import subprocess,tempfile,struct
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='demo-assets-') as folder:
 t=Path(folder);source=t/'test.c';exe=t/'test'
 source.write_text('''#include "demo_files.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv){assert(argc==2);const char *files[]={"Spool holder.gcode","Tool tray.gcode","Calibration cube.gcode"};
for(unsigned i=0;i<3;i++){char path[64],out[1024];size_t size=0;snprintf(path,sizeof(path),".demo/previews/%u.png",i);const unsigned char *png=demo_files_png(path,&size);assert(png && size>8);snprintf(out,sizeof(out),"%s/%u.png",argv[1],i);FILE *f=fopen(out,"wb");assert(f);assert(fwrite(png,1,size,f)==size);fclose(f);
int w=0,h=0;const unsigned short *pixels=demo_files_pixels(files[i],&w,&h);assert(pixels&&w==286&&h==165);snprintf(out,sizeof(out),"%s/%u.raw",argv[1],i);f=fopen(out,"wb");assert(f);assert(fwrite(pixels,2,w*h,f)==(size_t)(w*h));fclose(f);}return 0;}
''')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'main'),str(source),str(root/'main/demo_files.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(t)],check=True)
 originals=[]
 for i in range(3):
  image=Image.open(t/f'{i}.png');image.load();assert image.size==(900,520) and image.mode=='RGBA'
  rgb=Image.new('RGB',image.size,'#101b27');rgb.paste(image,mask=image.getchannel('A'));data=rgb.resize((286,165),Image.Resampling.LANCZOS).tobytes()
  expected=[((data[j]>>3)<<11)|((data[j+1]>>2)<<5)|(data[j+2]>>3) for j in range(0,len(data),3)]
  assert list(struct.unpack('<'+str(len(expected))+'H',(t/f'{i}.raw').read_bytes()))==expected
  originals.append(image.tobytes())
 assert len(set(originals))==3,'Sample artworks must be visibly different'
print('PASS: all three 900x520 PNG originals decode and exactly match their RGB565 previews')
