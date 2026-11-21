#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""awetab.py REF r0 r1 TEST t0 t1 [TEST44 for the alias column] : one before/after table row vs a real-card clip:
presence = mean test-ref dB over the 2.5-5 kHz 1/3-octave bands (kitrcmp), its max |dev| 2-6.3 kHz, 8 k / 10 k dev,
alias = (8-11 kHz minus 1-4 kHz level) of TEST vs TEST44 (same engine at 44.1 kHz): > 0 dB = folded-back energy."""
import sys, numpy as np
sys.path.insert(0, __import__('os').path.dirname(__file__))
from kitrcmp import load, align, third_oct, HOP, R
import wave
def ltas(fn, lo, hi):
    w = wave.open(fn); r = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(float).reshape(-1, 2).mean(1)
    N = 8192 if r > 30000 else 4096; P = 0
    for i in range(0, len(x) - N, N): P = P + abs(np.fft.rfft(x[i:i + N] * np.hanning(N))) ** 2
    f = np.fft.rfftfreq(N, 1 / r); return 10 * np.log10(P[(f >= lo) & (f < hi)].sum() / N + 1e-9)
a = sys.argv[1:]
ref = load(a[0], float(a[1]), float(a[2])); tst = load(a[3], float(a[4]), float(a[5]))
s, lag, k = align(ref, tst, [0]); a0, b0 = max(0, -lag) * HOP, max(0, lag) * HOP; L = min(len(ref) - a0, len(tst) - b0)
fc, br = third_oct(ref[a0:a0 + L]); _, bt = third_oct(tst[b0:b0 + L]); d = bt - br
pres = d[(fc > 2400) & (fc < 5100)].mean(); mx = abs(d[(fc > 1900) & (fc < 6400)]).max()
d8 = d[np.argmin(abs(fc - 8000))]; d10 = d[np.argmin(abs(fc - 10079))]
al = ''
if len(a) > 6: al = '%+.1f' % ((ltas(a[3], 8000, 10900) - ltas(a[3], 1000, 4000)) - (ltas(a[6], 8000, 10900) - ltas(a[6], 1000, 4000)))
print('chroma %.2f | presence %+.1f | max|dev| 2-6.3k %.1f | 8k %+.1f | 10k %+.1f | alias %s' % (s, pres, mx, d8, d10, al or '-'))
