#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""chromadtw.py REF.wav TEST.wav - same notes, whatever the tempo? Chroma (100 ms) of both, dynamic time warping
(TEST may run faster or slower in places: an emulator below real time drops or stretches audio), mean chroma
correlation along the path over loud frames, and the average tempo ratio. Same song + same sounds ~0.85+; another
song ~0.4. For 86Box recordings, where REC.WAV is not in real time."""
import sys, wave, numpy as np
def load(fn):
    w = wave.open(fn); r = w.getframerate()
    return np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, w.getnchannels()).mean(1), r
def chroma(x, r):
    hop = r // 10; N = 4096 if r < 30000 else 8192; win = np.hanning(N); fr = np.fft.rfftfreq(N, 1 / r)
    ok = (fr > 55) & (fr < 4000); pc = np.round(69 + 12 * np.log2(fr[ok] / 440)).astype(int) % 12
    C, E = [], []
    for i in range(0, len(x) - N, hop):
        S = np.abs(np.fft.rfft(x[i:i + N] * win)) ** 2
        c = np.bincount(pc, S[ok], 12); C.append(c); E.append(S.sum())
    C = np.array(C); C = (C - C.mean(1, keepdims=True)) / (C.std(1, keepdims=True) + 1e-12)
    return C, np.array(E)
(a, ra), (b, rb) = load(sys.argv[1]), load(sys.argv[2])
A, ea = chroma(a, ra); B, eb = chroma(b, rb)
la, lb = ea > np.percentile(ea, 25), eb > np.percentile(eb, 25)
A, B = A[la], B[lb]                       # loud frames only (silence carries no notes)
S = A @ B.T / 12                          # correlation matrix
n, m = S.shape; D = np.full((n + 1, m + 1), np.inf); D[0, 0] = 0
cost = 1 - S
for i in range(1, n + 1):
    for j in range(max(1, int(i * m / n) - 150), min(m, int(i * m / n) + 150) + 1):
        D[i, j] = cost[i - 1, j - 1] + min(D[i - 1, j], D[i, j - 1], D[i - 1, j - 1])
i, j, path = n, m, []
while i > 0 and j > 0:
    path.append((i - 1, j - 1)); k = np.argmin([D[i - 1, j - 1], D[i - 1, j], D[i, j - 1]])
    i, j = (i - 1, j - 1) if k == 0 else (i - 1, j) if k == 1 else (i, j - 1)
c = np.mean([S[p] for p in path])
print('DTW chroma corr %.3f over %d/%d loud frames (tempo TEST/REF %.2f)' % (c, n, m, m / n))
