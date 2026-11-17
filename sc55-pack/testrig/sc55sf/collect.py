#!/usr/bin/env python3
"""collect.py - oracle sweep: per program (or drum kit), notes at several keys/velocities; saves per-voice chip state
curves (pitch, level, TVA, TVF, pan, sends, sample) to JSON for the table decoding (fit_*.py). GPL-2.0-or-later.
usage: collect.py out.json [bank] [progs...]   (bank 'drum' = channel 10, programs = kit numbers)"""
import sys, json, oprobe as probe, rommap as M
from concurrent.futures import ThreadPoolExecutor
KEYS = [36, 48, 60, 72, 84]; VELS = [127, 64]; ON, OFF = 1200, 900
def sample_of(addr0, r7):
    hi = (r7 >> 8) & 15
    best = None; bd = 1 << 30
    for si in range(1064):
        S = M.sample(si)
        if (S['off'] >> 20) != hi: continue
        d = addr0 - (S['off'] & 0xfffff)
        if 0 <= d < bd: bd = d; best = si
    return best, bd
def one(bank, prog, keys=KEYS, vels=VELS, drum=False):
    ch = 9 if drum else 0
    L = list(probe.BOOT)
    if bank and not drum: L.append('m b0 00 %x' % bank)
    L += ['m %x %x' % (0xc0 | ch, prog), 'w 200', 'log 3']
    seg = []; t = 0
    for k in keys:
        for v in vels:
            seg.append((k, v)); L += ['m %x %x %x' % (0x90 | ch, k, v), 'w %d' % ON, 'm %x %x 40' % (0x80 | ch, k), 'w %d' % OFF]
    vo, out = probe.run(L)
    starts = sorted(probe.run.starts)          # (frame, slot, start address) of every voice the firmware starts
    if not starts: return []
    t0 = starts[0][0]; seglen = (ON + OFF) * probe.RATE // 1000
    res = []
    for i, (k, v) in enumerate(seg):
        a = t0 + i * seglen - 320; b = a + seglen
        vl = []
        for st, s, ad in starts:
            if not (a <= st < a + 640): continue
            nxt = min([x[0] for x in starts if x[1] == s and x[0] > st] + [b])
            pts = [p for p in vo.get(s, []) if st <= p[0] < nxt]
            if not pts: continue
            si, d = sample_of(ad, pts[0][9])
            vl.append(dict(slot=s, start=(st - a - 320) * 1000 // probe.RATE, sample=si, sdist=d, r7=pts[0][9],
                curve=[[(p[0] - st) * 1000 // probe.RATE] + list(p[1:8]) for p in pts[::5]]))
        res.append(dict(key=k, vel=v, voices=vl))
    return res
if __name__ == '__main__':
    out = sys.argv[1]; bank = sys.argv[2] if len(sys.argv) > 2 else '0'
    drum = bank == 'drum'; bank = 0 if drum else int(bank)
    progs = [int(x) for x in sys.argv[3:]] or list(range(128))
    with ThreadPoolExecutor(4) as ex:
        R = list(ex.map(lambda p: (p, one(bank, p, drum=drum)), progs))
    json.dump({str(p): r for p, r in R}, open(out, 'w'))
