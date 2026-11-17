import sys, oprobe as probe
PP = 0x10000 + 36
def tvf(pokes, key=60, vel=127, on=1200):
    vo = probe.note(0, key, vel, on, 100, pokes=[(PP + a, b) for a, b in pokes])
    cc = probe.curve(vo, "tvf")
    if not cc: return [(0, -1)]
    s, c = cc[0]
    return c
def at(c, ms): return min(c, key=lambda p: abs(p[0] - ms))[1]
mode = sys.argv[1]
if mode == 'lev':
    for X in range(0, 128, 8):
        c = tvf([(41, [X] * 5), (46, [0] * 5)]); print('L=%02x tvf %04x' % (X, at(c, 600)))
if mode == 'base':
    for b in (35, 36, 37, 38, 39, 40):
        for X in (0x00, 0x20, 0x40, 0x60, 0x7f):
            c = tvf([(41, [0x7f] * 5), (46, [0] * 5), (b, [X])]); print('pp%d=%02x tvf %04x' % (b, X, at(c, 600)))
if mode == 'key':
    for k in (24, 36, 48, 60, 72, 84, 96):
        c = tvf([(41, [0x40] * 5), (46, [0] * 5)], key=k); print('key %d tvf %04x' % (k, at(c, 600)))
if mode == 'vel':
    for v in (1, 32, 64, 96, 127):
        c = tvf([(41, [0x40] * 5), (46, [0] * 5)], vel=v); print('vel %d tvf %04x' % (v, at(c, 600)))
if mode == 'table':
    import json
    from concurrent.futures import ThreadPoolExecutor
    jobs = [(b, X) for b in (0, 0x40, 0x7f) for X in range(0, 128, 4)]
    def f(j):
        b, X = j; c = tvf([(40, [b]), (41, [X] * 5), (46, [0] * 5)], on=400); return b, X, at(c, 300)
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(f, jobs))
    json.dump(R, open('/tmp/tvftab.json', 'w'))
    for b, X, v in R: print('%02x %02x %04x' % (b, X, v), end=' | ')
if mode == 'scan':
    from concurrent.futures import ThreadPoolExecutor
    base = [(40, [0x40]), (41, [0x40] * 5), (46, [0] * 5)]
    ref = at(tvf(base, on=400), 300)
    def f(b):
        if 40 <= b <= 50: return b, None, None
        r = []
        for X in (0x10, 0x70):
            r.append(at(tvf(base + [(b, [X])], on=400), 300))
        return b, r, None
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(f, range(88)))
    print('ref %04x' % ref, ' '.join('pp%d:%s' % (b, '/'.join('%x' % x for x in r)) for b, r, _ in R if r and any(x != ref for x in r)))
if mode == 'base33':
    import json
    from concurrent.futures import ThreadPoolExecutor
    def f(X): return X, at(tvf([(33, [X]), (40, [0x40]), (41, [0x40] * 5), (46, [0] * 5)], on=400), 300)
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(f, range(0, 128, 2)))
    json.dump(R, open('/tmp/tvf33.json', 'w')); print(' '.join('%02x:%04x' % r for r in R))
if mode == 'scanq':
    from concurrent.futures import ThreadPoolExecutor
    def q(pk):
        vo = probe.note(0, 60, 127, 300, 50, pokes=[(PP + a, b) for a, b in pk]); cc = probe.curve(vo, 'ctl')
        return cc[0][1][50][1] if cc else -1
    ref = q([])
    def f(b): return b, [q([(b, [X])]) for X in (0x00, 0x30, 0x7f)]
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(f, range(88)))
    print('ref %04x' % ref, ' '.join('pp%d:%s' % (b, '/'.join('%x' % x for x in r)) for b, r in R if any(x != ref for x in r)))
