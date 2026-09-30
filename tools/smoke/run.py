#!/usr/bin/env python3
"""Run one scripted step against the ROM.

usage: run.py FROM_STATE TO_STATE "cmd; cmd; ..."
  FROM_STATE: a save state in out/ to start from, or - to boot the ROM fresh
  TO_STATE:   save state written to out/ at the end
Commands (one per ;):
  wait N                 run N frames with no input
  press KEYS [N]         hold KEYS for N frames (default 2), then release 1 frame. KEYS: A B START SELECT
                         UP DOWN LEFT RIGHT L R, joined with + (R+START). Walking one tile = press DIR 16
  mash KEYS COUNT GAP    press KEYS COUNT times, GAP frames apart
  shot NAME.png          screenshot into out/
Env: SMOKE_ROM (default ../../pokeemerald.gba), SHOTLOG=1 prints the emulator log to stderr.
"""
import os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, 'out')
os.makedirs(out, exist_ok=True)
rom = os.path.abspath(os.environ.get('SMOKE_ROM', os.path.join(here, '..', '..', 'pokeemerald.gba')))
src, dst, cmds = sys.argv[1], sys.argv[2], sys.argv[3]
lines = [] if src == '-' else [f'load {src}']
lines += [c.strip() for c in cmds.split(';') if c.strip()]
lines.append(f'save {dst}')
with open(os.path.join(out, '_script.txt'), 'w') as f:
    f.write('\n'.join(lines) + '\n')
subprocess.run([os.path.join(here, 'shot'), rom, '_script.txt'], check=True, cwd=out)
