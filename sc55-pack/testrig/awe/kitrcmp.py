#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""kitrcmp.py REF.wav r0 r1 TEST.wav [t0 t1] [-k] - real-card clip (REF, e.g. KITR AAC) vs our render (TEST), same song.
Aligns TEST to REF by chroma (100 ms frames; time lag, and key shift with -k), then on the aligned span reports:
1/3-octave long-term spectrum (each normalised to its 125 Hz-8 kHz total; test - ref dB, bands to 12.5 kHz only: AAC),
level (RMS dBFS, dynamic range p95-p10 of 100 ms levels), L/R balance, L/R correlation (all, <500 Hz, 500-4k, >4k),
side/mid dB, rests (p10 of 10 ms frames vs median, = reverb fill), -20 dB decay ms (median), attack (median 10-90 %
rise ms of 5 ms envelope at onsets). JSON line at the end (-j) for tables."""
import sys, json, numpy as np, wave
def load(fn, t0=None, t1=None):
    w = wave.open(fn); r = w.getframerate(); c = w.getnchannels(); n = w.getnframes()
    a0 = int((t0 or 0) * r); a1 = n if t1 is None else min(n, int(t1 * r)); w.setpos(a0)
    x = np.frombuffer(w.readframes(a1 - a0), np.int16).astype(np.float64).reshape(-1, c) / 32768
    if c == 1: x = np.repeat(x, 2, 1)
    if r != 44100:   # polyphase (linear interpolation drooped 22050 Hz renders by 3.5 dB at 8 kHz)
        from scipy.signal import resample_poly; from math import gcd; g = gcd(44100, r)
        x = resample_poly(x, 44100 // g, r // g, axis=0)
    return x
R = 44100; HOP = 4410
def chroma(x):
    m = x.mean(1); N = 8192; win = np.hanning(N); f = np.fft.rfftfreq(N, 1 / R); out = []
    sel = (f > 80) & (f < 4000); pc = (np.round(12 * np.log2(f[sel] / 440)) % 12).astype(int)
    for i in range(0, len(m) - N, HOP):
        S = np.abs(np.fft.rfft(m[i:i + N] * win))[sel] ** 2; c = np.bincount(pc, S, 12); out.append(c / (c.sum() + 1e-12))
    c = np.array(out); c = np.log(c + 1e-3); c -= c.mean(1, keepdims=True); return c / (np.linalg.norm(c, axis=1, keepdims=True) + 1e-9)
def align(ref, tst, keys):
    cr, ct = chroma(ref), chroma(tst); best = (-9, 0, 0)
    for k in keys:
        ck = np.roll(ct, -k, 1)
        for lag in range(-len(cr) + 20, len(ct) - 20):
            a0, b0 = max(0, -lag), max(0, lag); L = min(len(cr) - a0, len(ct) - b0)
            if L < min(50, len(cr) // 2): continue
            s = (cr[a0:a0 + L] * ck[b0:b0 + L]).sum(1).mean()
            if s > best[0]: best = (s, lag, k)
    return best
def third_oct(x):
    fc = 1000 * 2 ** (np.arange(-13, 12) / 3); N = 16384; f = np.fft.rfftfreq(N, 1 / R); P = np.zeros(len(f)); n = 0
    for i in range(0, len(x) - N, N // 2):
        P += np.abs(np.fft.rfft(x[i:i + N].mean(1) * np.hanning(N))) ** 2; n += 1
    P /= max(n, 1); b = np.array([P[(f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))].sum() for c in fc])
    norm = P[(f >= 125) & (f < 8000)].sum(); return fc, 10 * np.log10(b / norm + 1e-15)
def bandpass(x, lo, hi):
    X = np.fft.rfft(x, axis=0); f = np.fft.rfftfreq(len(x), 1 / R); X[(f < lo) | (f >= hi)] = 0; return np.fft.irfft(X, len(x), axis=0)
def stats(x):
    d = {}; lev = lambda v: 10 * np.log10((v ** 2).mean() + 1e-15)
    d['rms'] = lev(x); e = np.array([lev(x[i:i + HOP]) for i in range(0, len(x) - HOP, HOP)]); d['dr'] = np.percentile(e, 95) - np.percentile(e, 10)
    d['bal'] = lev(x[:, 0]) - lev(x[:, 1])
    cor = lambda v: float((v[:, 0] * v[:, 1]).sum() / np.sqrt((v[:, 0] ** 2).sum() * (v[:, 1] ** 2).sum() + 1e-15))
    d['corr'] = cor(x)
    for nm, lo, hi in (('corr_lo', 20, 500), ('corr_mid', 500, 4000), ('corr_hi', 4000, 12800)): d[nm] = cor(bandpass(x, lo, hi))
    d['side'] = lev((x[:, 0] - x[:, 1]) / 2) - lev((x[:, 0] + x[:, 1]) / 2)
    h = 441; f10 = np.array([lev(x[i:i + h]) for i in range(0, len(x) - h, h)])
    d['rest'] = np.percentile(f10, 10) - np.median(f10)
    dec = []
    for i in range(1, len(f10) - 60):   # after a 10 ms frame that is a local max >= median+3, frames until -20 dB
        if f10[i] > np.median(f10) + 3 and f10[i] >= f10[i - 1] and f10[i] >= f10[i + 1]:
            j = np.argmax(f10[i:i + 60] < f10[i] - 20)
            if j: dec.append(j * 10)
    d['decay20'] = float(np.median(dec)) if dec else -1
    h5 = 220; e5 = np.array([np.sqrt((x[i:i + h5] ** 2).mean()) for i in range(0, len(x) - h5, h5)]); rise = []
    for i in range(4, len(e5) - 12):
        if e5[i] > 2 * e5[i - 4] + 1e-5 and e5[i] > e5[i - 1]:
            pk = e5[i:i + 12].max(); base = e5[i - 4]; seg = e5[i - 4:i + 12]
            a = np.argmax(seg >= base + 0.1 * (pk - base)); b = np.argmax(seg >= base + 0.9 * (pk - base))
            if b > a: rise.append((b - a) * 5)
    d['attack'] = float(np.median(rise)) if rise else -1
    return d
if __name__ == '__main__':
    a = [v for v in sys.argv[1:] if not v.startswith('-')]; keys = range(-3, 4) if '-k' in sys.argv else [0]
    ref = load(a[0], float(a[1]), float(a[2])); tst = load(a[3], *(map(float, a[4:6]) if len(a) > 4 else ()))
    s, lag, k = align(ref, tst, keys); off = lag * HOP / R
    a0, b0 = max(0, -lag) * HOP, max(0, lag) * HOP; L = min(len(ref) - a0, len(tst) - b0); ref, tst = ref[a0:a0 + L], tst[b0:b0 + L]
    print('align: chroma corr %.2f, test = ref %+.2f s, key %+d, span %.1f s' % (s, off, k, L / R))
    fc, br = third_oct(ref); _, bt = third_oct(tst); sel = (fc >= 40) & (fc <= 12600)
    print('1/3 oct Hz :', ' '.join('%5d' % c for c in fc[sel]))
    print('test-ref dB:', ' '.join('%+5.1f' % v for v in (bt - br)[sel]))
    sr, st = stats(ref), stats(tst)
    print('%-9s %8s %8s' % ('', 'ref', 'test'))
    for kk in sr: print('%-9s %8.2f %8.2f' % (kk, sr[kk], st[kk]))
    if '-j' in sys.argv:
        print(json.dumps({'corr': s, 'off': off, 'key': k, 'fc': list(fc[sel]), 'diff': list((bt - br)[sel]), 'ref': sr, 'test': st}))
