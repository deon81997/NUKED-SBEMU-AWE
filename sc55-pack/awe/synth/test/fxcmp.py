#!/usr/bin/env python3
"""fxcmp.py OURS_DIR REF_DIR [NOTE_END_S]: reverb/chorus check per t12_rev*/t13_cho* render (ours vs 86Box):
level, tail level and decay time (-30 dB after the note), L/R correlation, chorus modulation depth/rate."""
import sys, os, wave, numpy as np
def load(p):
    w = wave.open(p); x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float).reshape(-1, 2)
    return x, w.getframerate()
def db(x): return 20 * np.log10(np.sqrt((x ** 2).mean()) / 32768 + 1e-12)
def env(m, r, ms=10):
    h = int(r * ms / 1000); n = len(m) // h
    return 20 * np.log10(np.sqrt((m[:n * h].reshape(n, h) ** 2).mean(1)) / 32768 + 1e-12)
def stats(p):
    x, r = load(p); m = x.mean(1); name = os.path.basename(p)
    if name.startswith("t12"):
        end = int(r * 0.30)                      # note off at 250 ms + release
        e = env(m[end:], r); pk = e[:5].max()
        t30 = next((i * 10 for i, v in enumerate(e) if v < pk - 30), len(e) * 10)
        return "all %6.1f  tail %6.1f dB  -30dB after %4d ms  L/R corr %.2f" % (
            db(x), db(x[end:]), t30, np.corrcoef(x[end:, 0], x[end:, 1])[0, 1])
    seg = m[int(r * 0.5):int(r * 3.0)]          # sustained part: modulation of the 10 ms envelope
    e = env(seg, r); e = e - e.mean(); sp = np.abs(np.fft.rfft(e)); f = np.fft.rfftfreq(len(e), 0.01)
    k = 1 + np.argmax(sp[1:])
    return "all %6.1f  env mod %.1f dB at %.2f Hz  L/R corr %.2f" % (
        db(x), e.std() * 2.83, f[k], np.corrcoef(x[int(r*0.5):int(r*3), 0], x[int(r*0.5):int(r*3), 1])[0, 1])
o, ref = sys.argv[1], sys.argv[2]
for n in sorted(os.listdir(o)):
    if n.startswith(("t12_rev", "t13_")) and n.endswith(".wav") and os.path.exists(os.path.join(ref, n)):
        print("%-24s ours %s\n%-24s 86Bx %s" % (n[:-4], stats(os.path.join(o, n)), "", stats(os.path.join(ref, n))))
