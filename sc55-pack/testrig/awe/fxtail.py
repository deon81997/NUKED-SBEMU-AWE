#!/usr/bin/env python3
# fxtail.py WAV t0 t1 [WAV t0 t1 ...]: how much reverb/chorus a music recording has: the quiet 10 ms frames (rests) relative
# to the median (reverb fills rests: closer to 0 dB = more tail), the share of near-silent frames, how fast the level
# falls after a drop (-20 dB decay time, median), and the L/R correlation (chorus/reverb widen the stereo image).
import sys, wave, numpy as np
a = sys.argv[1:]
for k in range(0, len(a), 3):
    w = wave.open(a[k]); r = w.getframerate(); w.setpos(int(float(a[k+1]) * r))
    x = np.frombuffer(w.readframes(int((float(a[k+2]) - float(a[k+1])) * r)), dtype=np.int16).reshape(-1, 2).astype(float)
    m = x.mean(1); h = r // 100; n = len(m) // h
    e = 20 * np.log10(np.sqrt((m[:n*h].reshape(n, h) ** 2).mean(1)) / 32768 + 1e-9)
    med = np.median(e); p10 = np.percentile(e, 10)
    dec = []
    for i in range(1, n - 60):
        if e[i-1] > med and e[i] < e[i-1] - 3:          # a note ends: time to fall 20 dB below the level before
            j = next((j for j in range(i, min(n, i + 100)) if e[j] < e[i-1] - 20), None)
            if j: dec.append((j - i + 1) * 10)
    print("%-40s rests %+5.1f dB vs median, silent %4.1f%%, -20 dB fall %4s ms, L/R corr %.2f" % (
        a[k][-40:], p10 - med, 100 * (e < med - 40).mean(), int(np.median(dec)) if dec else '-', np.corrcoef(x[:, 0], x[:, 1])[0, 1]))
