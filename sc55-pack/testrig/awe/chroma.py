#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""chroma.py A.wav B.wav [SECS] - same music? Chroma (12 pitch classes, 100 ms frames) correlation after aligning
the onset envelopes, plus onset-envelope correlation and level difference. Two renders of the same MIDI with the same
sounds give ~0.9+; wrong notes/pitches drop it (another song ~0.3-0.5)."""
import sys, wave, numpy as np
def load(fn, secs):
    w = wave.open(fn); r = w.getframerate(); n = min(w.getnframes(), int(secs * r))
    a = np.frombuffer(w.readframes(n), dtype='<i2').reshape(-1, w.getnchannels()).mean(1)
    return a, r
secs = float(sys.argv[3]) if len(sys.argv) > 3 else 120
A, ra = load(sys.argv[1], secs); B, rb = load(sys.argv[2], secs)
def feats(x, r):
    hop = r // 10; N = 8192; win = np.hanning(N); fr = np.fft.rfftfreq(N, 1 / r)
    ok = (fr > 55) & (fr < 4000); midi = 69 + 12 * np.log2(fr[ok] / 440); pc = np.round(midi).astype(int) % 12
    C, E = [], []
    for i in range(0, len(x) - N, hop):
        S = np.abs(np.fft.rfft(x[i:i + N] * win)) ** 2
        c = np.bincount(pc, S[ok], 12); C.append(c / (c.sum() + 1e-9)); E.append(np.log(S.sum() + 1))
    return np.array(C), np.array(E)
ca, ea = feats(A, ra); cb, eb = feats(B, rb)
oa, ob = np.maximum(np.diff(ea), 0), np.maximum(np.diff(eb), 0)
best, lag = -2, 0
for L in range(-30, 31):
    a, b = (oa[L:], ob) if L >= 0 else (oa, ob[-L:])
    n = min(len(a), len(b)); c = np.corrcoef(a[:n], b[:n])[0, 1]
    if c > best: best, lag = c, L
a, b = (ca[lag:], cb) if lag >= 0 else (ca, cb[-lag:])
n = min(len(a), len(b)); a, b = a[:n], b[:n]
loud = (np.r_[ea[max(lag,0):]][:n] > np.percentile(ea, 30))
cc = np.mean([np.corrcoef(a[i], b[i])[0, 1] for i in range(n) if loud[i]])
print('lag %+d00 ms  onset corr %.3f  chroma corr %.3f  level B-A %+.1f dB' % (lag, best, cc,
      10 * np.log10((B.astype(float) ** 2).mean() / (A.astype(float) ** 2).mean())))
