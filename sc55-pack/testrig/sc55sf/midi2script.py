#!/usr/bin/env python3
"""midi2script.py SONG.mid OUT.wav [seconds] > script.txt - an oracle script that renders a MIDI file (reference WAV)."""
import sys, struct
d = open(sys.argv[1], 'rb').read(); ntr, div = struct.unpack('>HH', d[10:14]); p = 14; raw = []
def vlq(q):
    v = 0
    while True:
        x = d[q]; q += 1; v = v << 7 | (x & 127)
        if not x & 128: return v, q
for t in range(ntr):
    ln = struct.unpack('>I', d[p + 4:p + 8])[0]; q = p + 8; e = q + ln; tick = 0; rs = 0
    while q < e:
        dt, q = vlq(q); tick += dt; s = d[q]
        if s == 0xFF:
            ty = d[q + 1]; l, q = vlq(q + 2)
            if ty == 0x51: raw.append((tick, 1, d[q] << 16 | d[q + 1] << 8 | d[q + 2]))
            q += l
            if ty == 0x2F: break
            continue
        if s in (0xF0, 0xF7):
            l, q2 = vlq(q + 1); raw.append((tick, 2, (b'\xf0' if s == 0xF0 else b'') + d[q2:q2 + l])); q = q2 + l; continue
        if s & 0x80: rs = s; q += 1
        n = 1 if rs & 0xF0 in (0xC0, 0xD0) else 2
        raw.append((tick, 2, bytes([rs]) + d[q:q + n])); q += n
    p = e
raw.sort(key=lambda r: (r[0], r[1]))
print('w 2500'); print('wav ' + sys.argv[2]); us = 0; last = 0; tempo = 500000; now = 0.0
lim = float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
for tick, k, v in raw:
    us += (tick - last) * tempo / div; last = tick
    if k == 1: tempo = v; continue
    t = us / 1000.0
    if t / 1000 > lim: break
    if t > now: print('w %.3f' % (t - now)); now = t
    print('m ' + ' '.join('%02x' % b for b in v))
print('w 3000')
