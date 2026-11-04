#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create the original SaveRelay icon/banner and a silent banner sound. Stdlib only."""
from pathlib import Path
import struct
import wave
import zlib
ROOT = Path(__file__).resolve().parent.parent / "assets"
FONT = {
'A':['01110','10001','10001','11111','10001','10001','10001'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'N':['10001','11001','11001','10101','10011','10011','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'Y':['10001','10001','01010','00100','00100','00100','00100']}
NAVY=(22,32,53); CYAN=(83,218,224); WHITE=(248,250,255); PANEL=(32,46,68)
def png(path,w,h,data):
 def chunk(kind,p):return struct.pack('>I',len(p))+kind+p+struct.pack('>I',zlib.crc32(kind+p))
 raw=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
def image(w,h):
 data=bytearray(NAVY)*(w*h)
 def rect(x,y,ww,hh,color):
  for yy in range(max(0,y),min(h,y+hh)):
   for xx in range(max(0,x),min(w,x+ww)):data[(yy*w+xx)*3:(yy*w+xx+1)*3]=bytes(color)
 def text(x,y,s,scale=1):
  for ch in s:
   for yy,row in enumerate(FONT.get(ch,['00000']*7)):
    for xx,v in enumerate(row):
     if v=='1':rect(x+xx*scale,y+yy*scale,scale,scale,WHITE)
   x+=6*scale
 return data,rect,text
def device(rect,x,y,scale):
 rect(x,y,18*scale,26*scale,CYAN);rect(x+2*scale,y+2*scale,14*scale,9*scale,PANEL);rect(x+2*scale,y+14*scale,14*scale,10*scale,PANEL)
 rect(x+4*scale,y+17*scale,2*scale,5*scale,WHITE);rect(x+3*scale,y+18*scale,4*scale,2*scale,WHITE)
for size in [48]:
 data,r,t=image(size,size);device(r,3,11,1);device(r,27,11,1);r(20,21,9,3,WHITE);r(25,19,3,7,WHITE);png(ROOT/'icon.png',size,size,data)
data,r,t=image(256,128);r(0,0,256,4,CYAN);t(74,17,'SAVERELAY',2);t(74,43,'LOCAL SAVE TRANSFER');device(r,50,67,2);device(r,170,67,2);r(103,89,48,5,CYAN);r(142,83,6,17,CYAN);png(ROOT/'banner.png',256,128,data)
with wave.open(str(ROOT/'banner.wav'),'wb') as sound:
 sound.setparams((2,2,32000,0,'NONE','not compressed'));sound.writeframes(b'\0'*(32000*4))
