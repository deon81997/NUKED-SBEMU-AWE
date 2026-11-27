#!/usr/bin/env python3
"""cmp.py A.RAW B.RAW : s16 stereo 32 kHz renders -> identical? else diff stats (SNR, max, first/last diff s, per-second RMS dB of the diff)"""
import sys, numpy as np
a = np.fromfile(sys.argv[1], '<i2').astype(np.int64); b = np.fromfile(sys.argv[2], '<i2').astype(np.int64)
n = min(len(a), len(b)); a, b = a[:n], b[:n]; d = a - b; nz = np.nonzero(d)[0]
if len(a) != len(b) or len(nz) == 0: print('IDENTICAL' if len(nz) == 0 and len(a) == len(b) else 'LENGTH DIFF')
if len(nz):
    snr = 10 * np.log10((a.astype(float) ** 2).sum() / (d.astype(float) ** 2).sum())
    print(f'samples={n} differ={len(nz)} ({100*len(nz)/n:.3f}%) max|d|={abs(d).max()} SNR={snr:.1f} dB '
          f'first={nz[0]//2/32000:.3f}s last={nz[-1]//2/32000:.3f}s')
    sec = 64000; r = [(i // sec, np.sqrt((d[i:i+sec].astype(float)**2).mean())) for i in range(0, n, sec)]
    worst = sorted(r, key=lambda x: -x[1])[:5]
    print('worst seconds (diff RMS):', ', '.join(f'{s}s {v:.1f}' for s, v in worst))
