# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
# cmp.py A.wav a0 a1 B.wav b0 b1 : align B to A (envelope xcorr), report level offset, envelope corr, band spectra, SNR after gain match
import sys, wave, numpy as np
from scipy.signal import resample_poly, correlate
def load(fn, t0, t1, R=44100):
    w = wave.open(fn); r = w.getframerate(); w.setpos(int(t0*r))
    a = np.frombuffer(w.readframes(int((t1-t0)*r)), dtype=np.int16).reshape(-1, 2).astype(np.float64)
    if r != R: a = resample_poly(a, R, r, axis=0)
    return a
A = load(sys.argv[1], float(sys.argv[2]), float(sys.argv[3])); B = load(sys.argv[4], float(sys.argv[5]), float(sys.argv[6]))
R = 44100; hop = 441
def env(x): m = x.mean(1); return np.array([np.sqrt((m[i:i+hop]**2).mean()) for i in range(0, len(m)-hop, hop)])
ea, eb = env(A), env(B)
la, lb = np.log(ea+1), np.log(eb+1)
c = correlate(la-la.mean(), lb-lb.mean(), mode='full'); lag = int(np.argmax(c) - (len(lb)-1))
print('env lag %+d ms (B relative to A)' % (lag*10))
# align
if lag >= 0: a2, b2 = A[lag*hop:], B
else: a2, b2 = A, B[-lag*hop:]
n = min(len(a2), len(b2)); a2, b2 = a2[:n], b2[:n]
ea2, eb2 = env(a2), env(b2); m = min(len(ea2), len(eb2))
print('envelope correlation (10 ms, log): %.3f' % np.corrcoef(np.log(ea2[:m]+1), np.log(eb2[:m]+1))[0,1])
ra, rb = np.sqrt((a2**2).mean()), np.sqrt((b2**2).mean())
print('RMS A %.1f dBFS  B %.1f dBFS  (B-A %.1f dB)' % (20*np.log10(ra/32768), 20*np.log10(rb/32768), 20*np.log10(rb/ra)))
# fine sample alignment
x, y = a2.mean(1), b2.mean(1)
cc = correlate(x[:R*5], y[:R*5], mode='full', method='fft'); fl = int(np.argmax(np.abs(cc)) - (R*5-1))
print('fine lag %+d samples, peak norm corr %.3f' % (fl, np.max(np.abs(cc))/np.sqrt((x[:R*5]**2).sum()*(y[:R*5]**2).sum())))
def bands(s):
    S = np.abs(np.fft.rfft(s*np.hanning(len(s))))**2; fr = np.fft.rfftfreq(len(s), 1/R)
    bb = [0,100,200,400,800,1600,3200,6400,12800,22050]; t = S.sum()
    return [10*np.log10(S[(fr>=bb[i])&(fr<bb[i+1])].sum()/t+1e-12) for i in range(len(bb)-1)]
ba, bbb = bands(x), bands(y)
print('band dB rel. total   A:', ' '.join('%5.1f' % v for v in ba)); print('                     B:', ' '.join('%5.1f' % v for v in bbb))
# left/right balance
print('L/R dB  A %.1f  B %.1f' % (20*np.log10(np.sqrt((a2[:,0]**2).mean())/np.sqrt((a2[:,1]**2).mean())), 20*np.log10(np.sqrt((b2[:,0]**2).mean())/np.sqrt((b2[:,1]**2).mean()))))
