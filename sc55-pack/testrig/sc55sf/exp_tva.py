#!/usr/bin/env python3
"""exp_tva.py - TVA decode experiments on Piano 1 partial 0 (mk1 v1.21 control ROM): poke levels/times, measure."""
import sys, oprobe as probe
PP = 0x10000 + 36          # pp[] of instrument 0 partial 0
def env(levels, times, on=1500, off=1500, key=60, vel=127):
    pk = [(PP + 70, levels), (PP + 74, times)]
    vo = probe.note(0, key, vel, on, off, pokes=pk)
    s, c = probe.curve(vo, 'tva')[0]
    return c
def t_reach(c, frac, t0=0, up=True):
    for t, v in c:
        if t >= t0 and ((v >= frac) if up else (v <= frac)): return t
    return None
if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'attack':
        for T in [0, 1, 2, 4, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 96, 112, 127]:
            for fl in (0, 0x80):
                c = env([0x7f, 0x7f, 0x7f, 0x7f], [T | fl, 0, 0, 0, 0x20], on=6000, off=100)
                pk = max(v for t, v in c)
                print('T1=%02x peak %5x t50 %s t90 %s t99 %s' % (T | fl, pk, t_reach(c, pk * .5), t_reach(c, pk * .9), t_reach(c, pk * .99)))
    if mode == 'rate':
        for T in (0x90, 0xa0, 0xb0, 0x20, 0x30):
            for L2 in (0x60, 0x40, 0x20, 0x00):
                c = env([0x7f, L2, L2, L2], [0, T, 0, 0, 0x20], on=4000, off=100)
                hi = c[3][1]; lo = c[-80][1]
                tgt = lo + (hi - lo) * 0.1
                print('T2=%02x L2=%02x hi %5x end %5x  t90-of-drop %s' % (T, L2, hi, lo, t_reach(c, tgt, 2, up=False)))
    if mode == 'level':
        for L in range(0, 128, 8):
            c = env([L, L, L, L], [0, 0, 0, 0, 0x20], on=300, off=50)
            print('L=%02x %5x %.2f dB' % (L, c[100][1], probe.db(c[100][1])))
