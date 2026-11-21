#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""stwidth.py WAV t0 t1 [WAV t0 t1 ...] - where the stereo width comes from: pan (coherent L/R, level differences) or
reverb/chorus (incoherent L/R, no level difference). STFT 2048/512, per band (<500, 500-4k, 4k-12.8k Hz): energy-weighted
inter-channel coherence (|<L R*>| over 8 frames) and the spread of the level difference (ILD p90-p10 dB, energy-weighted)."""
import sys, numpy as np
sys.path.insert(0, __import__('os').path.dirname(__file__))
from kitrcmp import load, R
def wq(v, w, q):
    o = np.argsort(v); c = np.cumsum(w[o]); return v[o][np.searchsorted(c, q * c[-1])]
a = sys.argv[1:]
for k in range(0, len(a), 3):
    x = load(a[k], float(a[k + 1]), float(a[k + 2])); N = 2048; H = 512; win = np.hanning(N)
    fr = [np.fft.rfft(x[i:i + N] * win[:, None], axis=0) for i in range(0, len(x) - N, H)]; S = np.array(fr)  # T,F,2
    L, Rr = S[:, :, 0], S[:, :, 1]; f = np.fft.rfftfreq(N, 1 / R); K = 8
    sm = lambda v: np.array([v[i:i + K].sum(0) for i in range(0, len(v) - K, K // 2)])
    cLR, pL, pR = sm(L * np.conj(Rr)), sm(np.abs(L) ** 2), sm(np.abs(Rr) ** 2)
    out = []
    for lo, hi in ((60, 500), (500, 4000), (4000, 12800)):
        b = (f >= lo) & (f < hi); e = (pL + pR)[:, b].ravel(); coh = (np.abs(cLR[:, b]) / np.sqrt(pL[:, b] * pR[:, b] + 1e-20)).ravel()
        ild = 10 * np.log10((pL[:, b] + 1e-20) / (pR[:, b] + 1e-20)).ravel(); m = e > e.max() * 1e-6
        out.append('%d-%d coh %.2f ild %.1f' % (lo, hi, (coh[m] * e[m]).sum() / e[m].sum(), wq(ild[m], e[m], .9) - wq(ild[m], e[m], .1)))
    print('%s %s-%s: %s' % (a[k].split('/')[-1], a[k + 1], a[k + 2], ' | '.join(out)))
