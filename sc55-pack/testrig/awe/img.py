# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
import sys, wave, numpy as np
fn, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
w = wave.open(fn); r = w.getframerate(); w.setpos(int(t0*r))
a = np.frombuffer(w.readframes(int((t1-t0)*r)), dtype=np.int16).reshape(-1, 2).astype(np.float64).mean(1)
S = np.log(np.abs(np.fft.rfft(a*np.hanning(len(a))))+1); df = r/len(a)
lowmax = int(3000/df); L = S[:lowmax]
best = []
for F in np.arange(15000, 26000, 2.0):   # image f' = F - f (mirror) or F + f (shift)
    for kind in ('mirror','shift'):
        idx = (F - np.arange(lowmax)*df) if kind=='mirror' else (F + np.arange(lowmax)*df)
        ii = np.round(idx/df).astype(int); ok = (ii>0)&(ii<len(S))
        if ok.sum() < lowmax*0.8: continue
        c = np.corrcoef(L[ok], S[ii[ok]])[0,1]; best.append((c, F, kind))
best.sort(reverse=True); print(best[:5])
