#!/usr/bin/env python3
"""exp_times.py - TVA stage time per time byte (0-127, both shape flags), attack (0 -> 127) and decay (127 -> 0x20)."""
import json, math, oprobe as probe
from concurrent.futures import ThreadPoolExecutor
PP = 0x10000 + 36
def meas(args):
    kind, T = args
    if kind == 'att': lv, tm = [0x7f] * 4, [T, 0, 0, 0, 0x20]
    else: lv, tm = [0x7f, 0x20, 0x20, 0x20], [0, T, 0, 0, 0x20]
    vo = probe.note(0, 60, 127, 7000 if T < 0xe8 and (T & 0x7f) < 0x68 else 12000, 50, pokes=[(PP + 70, lv), (PP + 74, tm)])
    s, c = probe.curve(vo, 'tva')[0]
    vals = [v for t, v in c]; ts = [t for t, v in c]
    hi = 0x3980; lo = vals[-20] if kind == 'dec' else 0
    out = {}
    for f in (0.1, 0.5, 0.9, 0.99):
        tgt = (lo + (hi - lo) * f) if kind == 'att' else (hi - (hi - lo) * f)
        out[f] = next((t for t, v in c if (v >= tgt if kind == 'att' else v <= tgt)), None)
    if kind == 'dec':  # dB domain 90%
        dbh, dbl = probe.db(hi), probe.db(max(lo, 1)); tg = dbh + (dbl - dbh) * 0.9
        out['db90'] = next((t for t, v in c if probe.db(v) <= tg), None)
    return kind, T, out
if __name__ == '__main__':
    jobs = [(k, T | f) for k in ('att', 'dec') for f in (0, 0x80) for T in range(128)]
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(meas, jobs))
    json.dump([[k, T, {str(a): b for a, b in o.items()}] for k, T, o in R], open('/tmp/times.json', 'w'))
    for k, T, o in R:
        if T % 8 == 0: print(k, hex(T), o)
