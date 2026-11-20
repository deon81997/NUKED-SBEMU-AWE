#!/usr/bin/env python3
# pvdiff.py A.p B.p : per-voice diff of two harness -P probes (frame ch pitch vol cut addr every 64 frames).
# Per channel: frames sounding in A / B / both, pitch |cents| (median, p99), volume dB (median, p99 of |diff|, only
# where both > -60 dB), cutoff register units, sample address drift (words) - the worst lines first.
import sys, numpy as np, math
def load(p):
    a = np.loadtxt(p, dtype=np.int64, ndmin=2)
    return {(int(r[0]), int(r[1])): r[2:] for r in a}
A, B = load(sys.argv[1]), load(sys.argv[2])
keys = set(A) | set(B); rows = []
tot = {'pitch': [], 'vol': [], 'cut': [], 'addr': []}
for ch in range(32):
    ka = [k for k in A if k[1] == ch]; kb = [k for k in B if k[1] == ch]; kk = [k for k in ka if k in B]
    if not ka and not kb: continue
    p = []; v = []; c = []; d = []
    for k in kk:
        a, b = A[k], B[k]
        if a[0] and b[0]: p.append(abs(1200 * math.log2(a[0] / b[0])))
        if a[1] > 66 and b[1] > 66: v.append(abs(20 * math.log10(a[1] / b[1])))
        c.append(abs(int(a[2]) - int(b[2]))); d.append(abs(int(a[3]) - int(b[3])))
    f = lambda x, q: float(np.percentile(x, q)) if x else 0.0
    rows.append((ch, len(ka), len(kb), len(kk), f(p, 50), f(p, 99), f(v, 50), f(v, 99), f(c, 50), f(c, 99), f(d, 50), f(d, 99)))
    tot['pitch'] += p; tot['vol'] += v; tot['cut'] += c; tot['addr'] += d
print('ch  onA   onB  both | cents med p99 | vol dB med p99 | cut med p99 | addr med p99')
for r in sorted(rows, key=lambda r: -(r[5] + 10 * r[7] + r[9] / 100)):
    print('%2d %5d %5d %5d | %6.2f %7.2f | %5.2f %6.2f | %5.0f %6.0f | %6.0f %8.0f' % r)
f = lambda x, q: float(np.percentile(x, q)) if x else 0.0
print('ALL: frames A %d B %d | cents med %.2f p99 %.2f | vol dB med %.2f p99 %.2f | cut med %.0f p99 %.0f | addr med %.0f p99 %.0f' % (
    len(A), len(B), f(tot['pitch'], 50), f(tot['pitch'], 99), f(tot['vol'], 50), f(tot['vol'], 99), f(tot['cut'], 50), f(tot['cut'], 99), f(tot['addr'], 50), f(tot['addr'], 99)))
