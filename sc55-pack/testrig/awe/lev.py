# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
import sys, wave, numpy as np
for fn in sys.argv[1:]:
    w = wave.open(fn); r = w.getframerate(); n = w.getnframes()
    a = np.frombuffer(w.readframes(n), dtype=np.int16).reshape(-1, 2).astype(np.float64)
    secs = n // r
    lv = []
    for i in range(secs):
        x = a[i*r:(i+1)*r]
        rms = np.sqrt((x**2).mean()) + 1e-9
        lv.append('%d' % max(-99, round(20*np.log10(rms/32768))))
    print(fn.split('/')[-1], r, 'Hz', secs, 's  dBFS/s:', ' '.join(lv))
