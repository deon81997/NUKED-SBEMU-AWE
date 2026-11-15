#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""mkswitch.py OUTDIR - MPUPLAY scripts for the MIDI output switch test (beta.123): SWA = handshake, GM + GS resets,
   a GS SysEx, every channel's bank / program / controllers / pitch bend range / pitch bend, then a song; SWB SWC SWD =
   more of the song only (no set-up), so after each switch the new output must have got the state from SBEMU."""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
R = random.Random(123)
def D(b): return 'D%02X' % b
def roland(addr, data):
    s = sum(addr) + sum(data)
    return [0xF0, 0x41, 0x10, 0x42, 0x12] + addr + data + [(128 - s % 128) % 128, 0xF7]
def song(n):
    t = []
    for i in range(n):
        ch = R.randrange(16); k = R.random()
        if k < 0.6: b = [0x90 | ch, R.randrange(30, 90), R.choice([0, R.randrange(1, 128)])]
        elif k < 0.8: b = [0x80 | ch, R.randrange(30, 90), 64]
        elif k < 0.9: b = [0xB0 | ch, R.choice([1, 7, 10, 11, 64]), R.randrange(128)]
        else: b = [0xE0 | ch, R.randrange(128), R.randrange(128)]
        t += [D(x) for x in b]
        if R.random() < 0.3: t.append('W%d' % R.choice([2, 5, 10]))
    return t
a = ['CFF', 'A', 'C3F', 'A'] + [D(x) for x in [0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7]] + ['W50']
a += [D(x) for x in roland([0x40, 0x00, 0x7F], [0x00])] + ['W50'] + [D(x) for x in roland([0x40, 0x01, 0x30], [0x04])]
for ch in range(16):
    a += [D(x) for x in [0xB0 | ch, 0, R.randrange(4), 32, 0, 0xC0 | ch, R.randrange(128), 0xB0 | ch, 7, R.randrange(60, 127),
                         10, R.randrange(128), 91, R.randrange(128), 101, 0, 100, 0, 6, R.randrange(1, 13), 38, 0, 101, 127, 100, 127,
                         0xE0 | ch, R.randrange(128), R.randrange(128)]]
a += song(400)
os.makedirs(sys.argv[1], exist_ok=True)
for name, t in (('SWA', a), ('SWB', song(400)), ('SWC', song(400)), ('SWD', song(400))):
    open(os.path.join(sys.argv[1], name + '.TXT'), 'w').write(' '.join(t + ['W2000']) + '\r\n')
