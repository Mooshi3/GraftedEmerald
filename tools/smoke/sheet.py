#!/usr/bin/env python3
"""Tile screenshots into one contact sheet: sheet.py OUT.png a.png b.png ... [--cols N] [--scale S]"""
import sys
from PIL import Image
args = sys.argv[1:]
cols, scale = 4, 1
if '--cols' in args:
    i = args.index('--cols'); cols = int(args[i + 1]); del args[i:i + 2]
if '--scale' in args:
    i = args.index('--scale'); scale = int(args[i + 1]); del args[i:i + 2]
dest, files = args[0], args[1:]
w, h = 240 * scale, 160 * scale
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (w * min(cols, len(files)), h * rows))
for i, f in enumerate(files):
    sheet.paste(Image.open(f).resize((w, h), Image.NEAREST), ((i % cols) * w, (i // cols) * h))
sheet.save(dest)
