# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
import sys, wave, numpy as np
fn, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
w = wave.open(fn); r = w.getframerate(); w.setpos(int(t0*r))
a = np.frombuffer(w.readframes(int((t1-t0)*r)), dtype=np.int16).reshape(-1, 2).astype(np.float64).mean(1)
hop = r//20; env = [20*np.log10(np.sqrt((a[i:i+hop]**2).mean())/32768+1e-9) for i in range(0, len(a)-hop, hop)]
print('50ms env dB: min %.1f max %.1f std %.1f' % (min(env), max(env), np.std(env)))
print(' '.join('%d' % e for e in env[:60]))
S = np.abs(np.fft.rfft(a * np.hanning(len(a)))); fr = np.fft.rfftfreq(len(a), 1/r)
bands = [0,100,200,400,800,1600,3200,6400,12800,22050]
tot = (S**2).sum()
print('band energy %:', ' '.join('%d-%d:%.1f' % (bands[i], bands[i+1], 100*(S[(fr>=bands[i])&(fr<bands[i+1])]**2).sum()/tot) for i in range(len(bands)-1)))
pk = fr[np.argsort(S)[-8:]]; print('top freqs', sorted(int(x) for x in pk))
