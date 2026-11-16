#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""dense.py OUT.MID [SECS] - note test for the AWE32 MIDI Synth: 15 melodic channels + drums, 3-note chords every
1/8 s per channel (~380 note-ons/s, far more than 30 voices: voice stealing), program changes, controllers, bend."""
import sys, struct
secs = int(sys.argv[2]) if len(sys.argv) > 2 else 30
def vlq(v):
    b = [v & 127]; v >>= 7
    while v: b.append(v & 127 | 128); v >>= 7
    return bytes(reversed(b))
ev = []   # (tick, bytes); 480 ticks = 0.5 s at 120 bpm -> 1/8 s = 120 ticks
for ch in range(16):
    if ch != 9: ev.append((0, bytes([0xC0 | ch, (ch * 8) % 128])))
    ev.append((0, bytes([0xB0 | ch, 7, 100])))
for step in range(secs * 8):
    t = step * 120
    for ch in range(16):
        if ch == 9:
            for k in (36, 42, 38 if step % 2 else 46): ev += [(t, bytes([0x99, k, 100])), (t + 60, bytes([0x89, k, 0]))]
            continue
        root = 36 + (ch * 5 + step) % 48
        for k in (root, root + 4, root + 7): ev += [(t, bytes([0x90 | ch, k, 90])), (t + 100, bytes([0x80 | ch, k, 0]))]
        if step % 16 == 8: ev.append((t, bytes([0xE0 | ch, 0, 0x50])))
        if step % 16 == 12: ev.append((t, bytes([0xE0 | ch, 0, 0x40])))
ev.sort(key=lambda e: e[0])
d, last = b'', 0
for t, m in ev: d += vlq(t - last) + m; last = t
d += vlq(480) + b'\xff\x2f\x00'
open(sys.argv[1], 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(d)) + d)
print('%s: %d note-ons' % (sys.argv[1], sum(1 for _, m in ev if m[0] & 0xF0 == 0x90)))
