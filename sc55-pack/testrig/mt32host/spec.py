#!/usr/bin/env python3
"""spec.py A.RAW B.RAW : level per 0.62 s slot (dB, A-B) + octave-band spectrum difference (dB) - tells phase-only drift from audible change"""
import sys, numpy as np
def load(p): x = np.fromfile(p, '<i2').astype(float).reshape(-1, 2).mean(1); return x
a, b = load(sys.argv[1]), load(sys.argv[2]); n = min(len(a), len(b)); a, b = a[:n], b[:n]
w = int(0.62 * 32000); lv = []
for i in range(0, n - w, w):
    ra, rb = np.sqrt((a[i:i+w]**2).mean()) + 1e-9, np.sqrt((b[i:i+w]**2).mean()) + 1e-9
    if ra > 30: lv.append(20 * np.log10(ra / rb))
lv = np.array(lv); print(f'slot level A-B: mean {lv.mean():+.2f} dB, max |{abs(lv).max():.2f}| dB over {len(lv)} slots, >0.5 dB in {int((abs(lv)>0.5).sum())}')
def bands(x):
    f = np.abs(np.fft.rfft(x[: (len(x)//65536)*65536].reshape(-1, 65536) * np.hanning(65536), axis=1))**2
    f = f.mean(0); fr = np.fft.rfftfreq(65536, 1/32000); out = []
    for lo in [31, 63, 125, 250, 500, 1000, 2000, 4000, 8000]:
        m = (fr >= lo) & (fr < lo * 2); out.append(10 * np.log10(f[m].sum()))
    return np.array(out)
d = bands(a) - bands(b); print('octave bands 31..8k Hz A-B dB:', ' '.join(f'{v:+.2f}' for v in d))
