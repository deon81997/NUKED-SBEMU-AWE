#!/usr/bin/env python3
"""oprobe.py - run the oracle (Nuked-SC55 + trace hooks) on a script and return per-voice state curves.
GPL-2.0-or-later, part of NUKED SB-EMU testrig/sc55sf. Used to decode the SC-55 control ROM tables (docs/SC55ROM.md)."""
import subprocess, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
ROMS = os.environ.get('SC55ROMS', '/tmp/dl/spx/tmp/p52/NUKEDSB/FILES/ROMS')
RATE = 64000
def run(lines, romset='auto', log=3, extra=''):
    import threading
    sc = '/tmp/sc55sf_probe_%d_%d.txt' % (os.getpid(), threading.get_ident())
    open(sc, 'w').write('\n'.join(lines) + '\n')
    out = subprocess.run([os.path.join(HERE, 'oracle'), ROMS, sc, romset], capture_output=True, text=True).stdout
    vo = {}; starts = []
    for l in out.splitlines():
        f = l.split()
        if f[0] == 'N':
            starts.append((int(f[1]), int(f[2]), int(f[3], 16)))
        if f[0] == 'S':
            t = int(f[1]); s = int(f[2])
            if s >= 24: continue
            v = [int(x, 16) for x in f[3:]]
            vo.setdefault(s, []).append((t,) + tuple(v))
    run.starts = starts
    return vo, out
BOOT = ['w 2500']
def note(prog=0, key=60, vel=100, on_ms=1000, off_ms=1000, ch=0, bank=0, pokes=(), pre=(), drum=False):
    L = list(BOOT)
    for a, b in pokes: L.append('poke %x %s' % (a, ' '.join('%x' % x for x in b)))
    L += list(pre)
    if bank: L.append('m %x 00 %x' % (0xb0 | ch, bank))
    L.append('m %x %x' % (0xc0 | ch, prog)); L.append('w 100'); L.append('log 3')
    L.append('m %x %x %x' % (0x90 | ch, key, vel)); L.append('w %d' % on_ms)
    L.append('m %x %x 40' % (0x80 | ch, key)); L.append('w %d' % off_ms)
    vo, out = run(L)
    return vo
FIELDS = dict(pitch=1, pan=2, send=3, ctl=4, lvl=5, tva=6, tvf=7, addr=8, r7=9)
def curve(vo, field='tva'):
    """the voices keyed after note-on: list of (slot, [(ms, value)])"""
    i = FIELDS[field]; r = []
    for s, pts in sorted(vo.items()):
        t0 = pts[0][0]
        r.append((s, [((p[0] - t0) * 1000.0 / RATE, p[i]) for p in pts]))
    return r
def db(v): return 20 * math.log10(max(v, 1) / 32768.0)
if __name__ == '__main__':
    vo = note(int(sys.argv[1]) if len(sys.argv) > 1 else 0)
    for s, c in curve(vo, 'tva'):
        print('slot', s, ' '.join('%.0f:%.1f' % (t, db(v)) for t, v in c[::25]))
