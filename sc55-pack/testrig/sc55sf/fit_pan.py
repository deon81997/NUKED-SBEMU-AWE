import sys, math, common as K, rommap as M, numpy as np
from collections import Counter, defaultdict
d = K.load(sys.argv[1]); g=defaultdict(list)
for prog, notes in d.items():
    ii = M.variation(0, int(prog))
    for n in notes:
        if n['vel']!=127: continue
        I, vp = K.voice_partials(ii, n['key'], n['voices'])
        for pn, v in vp:
            P = I['parts'][pn]['pp']; pan = K.med(K.col(v,'pan',20,500)); send=K.med(K.col(v,'send',20,500))
            L=(pan>>8)&255; R=pan&255
            g[(P[5], I['hdr'][5])].append((n['key'], L, R, send>>8, send&255, I['hdr'][3], I['hdr'][4], int(prog)))
for k in sorted(g):
    v=g[k]; print('pp5=%02x hdr5=%02x'%k, Counter((x[1],x[2]) for x in v).most_common(3), 'keys', sorted(set((x[0],x[1],x[2]) for x in v))[:5] if len(set((x[1],x[2]) for x in v))>1 else '')
