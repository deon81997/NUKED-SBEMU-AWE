#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""notes.py mid OUT.MID : note 60, 0.5 s, every program 0-127 (0.75 s apart)
   notes.py cmp REF.WAV TEST.WAV : per program: cents TEST-REF (harmonic product spectrum), rms of both"""
import sys, struct, wave, numpy as np
if sys.argv[1] == 'mid':
    def vlq(v):
        b = [v & 127]; v >>= 7
        while v: b.append(v & 127 | 128); v >>= 7
        return bytes(reversed(b))
    ev = b''
    for p in range(128): ev += vlq(0) + bytes([0xC0, p]) + vlq(0) + bytes([0x90, 60, 100]) + vlq(384) + bytes([0x80, 60, 0]) + vlq(192) + bytes([0xB0, 0x7B, 0])
    ev += vlq(0) + b'\xff\x2f\x00'
    open(sys.argv[2], 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 384) + b'MTrk' + struct.pack('>I', len(ev)) + ev)
    sys.exit(0)
def load(fn):
    w = wave.open(fn); return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, 2).mean(1), w.getframerate()
def f0(x, r):
    if len(x) < 256 or np.abs(x).max() < 30: return 0
    n = len(x); X = np.abs(np.fft.rfft((x - x.mean()) * np.hanning(n), 8 * n)); fr = np.fft.rfftfreq(8 * n, 1 / r)
    h = X.copy()
    for k in (2, 3): h[:len(X) // k] *= X[::k][:len(X) // k]
    ok = (fr > 40) & (fr < 3000); return fr[ok][np.argmax(h[ok])]
a, ra = load(sys.argv[2]); b, rb = load(sys.argv[3])
for i in range(128):
    sa = a[int((i * .75 + .1) * ra):int((i * .75 + .45) * ra)]; sb = b[int((i * .75 + .1) * rb):int((i * .75 + .45) * rb)]
    fa, fb = f0(sa, ra), f0(sb, rb)
    c = 1200 * np.log2(fb / fa) if fa and fb else 0
    print(i, int(round(c)), int(np.sqrt((sa ** 2).mean())) if len(sa) else 0, int(np.sqrt((sb ** 2).mean())) if len(sb) else 0)
