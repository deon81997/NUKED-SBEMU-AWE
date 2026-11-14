# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
import sys, wave, numpy as np
fn = sys.argv[1]; lo, hi = float(sys.argv[2]), float(sys.argv[3])
w = wave.open(fn); r = w.getframerate(); n = w.getnframes()
a = np.frombuffer(w.readframes(n), dtype=np.int16).reshape(-1, 2).astype(np.float64).mean(1)
out = []
for i in range(n // r):
    x = a[i*r:(i+1)*r]; S = np.abs(np.fft.rfft(x*np.hanning(r)))**2; fr = np.fft.rfftfreq(r, 1/r)
    e = S[(fr>=lo)&(fr<hi)].sum(); t = S.sum()+1e-9
    out.append('%d' % round(10*np.log10(e/t+1e-12)) if t > 1e3 else '.')
print(fn.split('/')[-3], ' '.join(out))
