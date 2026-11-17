"""common.py - helpers shared by the fit_*.py decoders (voice -> instrument partial, steady values)."""
import json, math, rommap as M
def load(fn): return json.load(open(fn))
def part_sample(pidx, key):
    P = M.part(pidx)
    for y in range(16):
        if key <= P['breaks'][y] or y == 15: return P['samples'][y]
def voice_partials(ii, key, voices):
    """map voices to (partial number, voice) by the sample each partial's part plays at this key"""
    I = M.inst(ii); out = []
    for v in voices:
        for pn, P in enumerate(I['parts']):
            if P['idx'] != 0xffff and part_sample(P['idx'], key) == v['sample'] and pn not in [o[0] for o in out]:
                out.append((pn, v)); break
    return I, out
def med(xs):
    xs = sorted(xs); return xs[len(xs) // 2] if xs else None
C = dict(t=0, pitch=1, pan=2, send=3, ctl=4, lvl=5, tva=6, tvf=7)
def col(v, name, t0=0, t1=1e9): return [c[C[name]] for c in v['curve'] if t0 <= c[0] < t1]
