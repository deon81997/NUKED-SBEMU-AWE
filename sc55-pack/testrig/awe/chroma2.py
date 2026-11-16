#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""chroma2.py A.wav B.wav [MAXLAG_S] - like chroma.py, but B is aligned on the chroma itself (best lag within
+-MAXLAG s, 100 ms steps; for recordings that start anywhere in the song) and the time scale is searched too
(0.97-1.03: a VM's clock). Prints lag, scale, chroma correlation (loud frames) and level B-A."""
import sys, wave, numpy as np
def load(fn):
    w = wave.open(fn); r = w.getframerate()
    return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, w.getnchannels()).mean(1), r
def chroma(x, r, hop_s=0.1):
    hop = int(r * hop_s); N = 8192 if r > 30000 else 4096; win = np.hanning(N); fr = np.fft.rfftfreq(N, 1 / r)
    ok = (fr > 55) & (fr < 4000); pc = np.round(69 + 12 * np.log2(fr[ok] / 440)).astype(int) % 12
    C, E = [], []
    for i in range(0, len(x) - N, hop):
        S = np.abs(np.fft.rfft(x[i:i + N] * win)) ** 2
        c = np.bincount(pc, S[ok], 12); C.append(c / (c.sum() + 1e-9)); E.append(S.sum())
    C = np.array(C); C = (C - C.mean(1, keepdims=True)) / (C.std(1, keepdims=True) + 1e-12)
    return C, np.array(E)
A, ra = load(sys.argv[1]); B, rb = load(sys.argv[2]); ml = int(float(sys.argv[3]) * 10) if len(sys.argv) > 3 else 200
ca, ea = chroma(A, ra); best = (-2, 0, 1.0)
for sc in (0.97, 0.98, 0.99, 1.0, 1.01, 1.02, 1.03):
    cb, eb = chroma(B, int(rb * sc))      # B read as if its rate were rb*sc: time scale
    loud = eb > np.percentile(eb, 30)
    for L in range(-ml, ml + 1):
        ia = np.arange(len(cb)) + L; m = (ia >= 0) & (ia < len(ca)) & loud
        if m.sum() < 50: continue
        c = np.mean(np.sum(ca[ia[m]] * cb[m], 1) / 12)
        if c > best[0]: best = (c, L, sc)
print('lag %+.1f s  scale %.2f  chroma corr %.3f  level B-A %+.1f dB' % (best[1] / 10, best[2], best[0],
      10 * np.log10((B.astype(float) ** 2).mean() / (A.astype(float) ** 2).mean())))
