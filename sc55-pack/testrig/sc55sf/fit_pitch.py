import sys, math, common as K, rommap as M
d = K.load(sys.argv[1]); rows = []
for prog, notes in d.items():
    ii = M.variation(0, int(prog))
    for n in notes:
        I, vp = K.voice_partials(ii, n['key'], n['voices'])
        for pn, v in vp:
            S = M.sample(v['sample']); P = I['parts'][pn]['pp']
            p = K.med(K.col(v, 'pitch', 100, 1000))
            if not p: continue
            c = 1200 * math.log2(p / 16384) - 100 * (n['key'] - S['root']) - (S['pitch'] - 1024) / 10
            rows.append((int(prog), I['name'], pn, n['key'], n['vel'], c, P))
for r in rows:
    if r[4] == 127: print('%3d %-12s p%d k%2d %+8.1f  pp6=%02x pp7=%02x pp8=%02x pp9=%02x pp10=%02x | %s' % (r[0], r[1], r[2], r[3], r[5], r[6][6], r[6][7], r[6][8], r[6][9], r[6][10], r[6][11:24].hex()))
