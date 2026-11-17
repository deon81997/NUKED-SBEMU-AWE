import sys, math, common as K, rommap as M, numpy as np
d = K.load(sys.argv[1]); rows=[]
def db(x): return 20*math.log10(max(x,1)/32768)
for prog, notes in d.items():
    ii = M.variation(0, int(prog))
    for n in notes:
        I, vp = K.voice_partials(ii, n['key'], n['voices'])
        for pn, v in vp:
            S = M.sample(v['sample']); P = I['parts'][pn]['pp']
            lv = K.col(v,'lvl',0,1000); tv=K.col(v,'tva',0,1100)
            rows.append(dict(prog=int(prog),pn=pn,key=n['key'],vel=n['vel'],lvl=db(K.med(lv)),lvl0=db(lv[1]),tvapk=db(max(tv)),
              h0=I['hdr'][0], p65=P[65], sv=S['vol'], fv=S['fvol'], L1=P[70], P=P, hdr=I['hdr']))
import collections
r127=[r for r in rows if r['vel']==127]
# variation of lvl within an instrument over keys?
by=collections.defaultdict(list)
for r in r127: by[(r['prog'],r['pn'])].append(r['lvl'])
print('lvl spread over keys (max-min) median', np.median([max(v)-min(v) for v in by.values()]))
X=[]; y=[]
for r in r127:
    X.append([40*math.log10(max(r['h0'],1)/127), 40*math.log10(max(r['p65'],1)/127), 1]); y.append(r['lvl'])
X=np.array(X); y=np.array(y); co=np.linalg.lstsq(X,y,rcond=None)[0]; e=X@co-y
print('coef h0 p65 c', co, 'rms', np.sqrt(np.mean(e**2)))
w=np.argsort(-np.abs(e))[:10]
for i in w: r=r127[i]; print('%3d p%d k%d lvl %.1f err %.1f h0 %02x p65 %02x sv %02x fv %d L1 %02x P60-70 %s' % (r['prog'],r['pn'],r['key'],r['lvl'],e[i],r['h0'],r['p65'],r['sv'],r['fv'],r['L1'],r['P'][56:70].hex()))
# velocity
dv=[]
for r in rows:
    if r['vel']==64:
        m=[q for q in r127 if q['prog']==r['prog'] and q['pn']==r['pn'] and q['key']==r['key']]
        if m: dv.append((r['lvl']-m[0]['lvl'], r['tvapk']-m[0]['tvapk'], r['prog'], r['P'][60:64].hex(), r['P'][66:70].hex()))
print('vel64 vs 127 dB lvl / tva: ', collections.Counter(round(x[0],0) for x in dv).most_common(8), collections.Counter(round(x[1],0) for x in dv).most_common(8))
print('---- lvl0 regression')
X=[];y=[];RR=[]
for r in r127:
    if r['lvl0']<-80: continue
    X.append([max(r['h0'],1), max(r['p65'],1), r['sv'], 1]); y.append(r['lvl0']); RR.append(r)
for f in (lambda v: v, lambda v: 20*math.log10(v/127), lambda v: 40*math.log10(v/127)):
    A=np.array([[f(x[0]),f(x[1]),f(max(x[2],1)),1] for x in X]); co=np.linalg.lstsq(A,np.array(y),rcond=None)[0]; e=A@co-np.array(y)
    print(co, np.sqrt(np.mean(e**2)))
from collections import Counter
print(Counter((r['h0'],r['p65'],r['sv'],round(r['lvl0'],1)) for r in RR).most_common(25))
print('---- tva peak regression (vel 127)')
X=[];y=[];RR=[]
for r in r127:
    if r['tvapk']<-80: continue
    X.append(r); y.append(r['tvapk'])
y=np.array(y)
for name,f in (('lin',lambda v: v),('20log',lambda v: 20*math.log10(max(v,1)/127)),('40log',lambda v: 40*math.log10(max(v,1)/127))):
    A=np.array([[f(x['h0']),f(x['p65']),f(x['sv']),f(x['L1']),1] for x in X]); co=np.linalg.lstsq(A,y,rcond=None)[0]; e=A@co-y
    print(name, np.round(co,3), 'rms %.2f'%np.sqrt(np.mean(e**2)))
f=lambda v: 40*math.log10(max(v,1)/127)
A=np.array([[f(x['h0']),f(x['p65']),f(x['sv']),f(x['L1']),1] for x in X]); co=np.linalg.lstsq(A,y,rcond=None)[0]; e=A@co-y
for i in np.argsort(-np.abs(e))[:12]:
    r=X[i]; print('%3d p%d k%d pk %.1f err %.1f h0 %02x p65 %02x sv %02x L1 %02x fv %d hdr %s P56-70 %s'%(r['prog'],r['pn'],r['key'],r['tvapk'],e[i],r['h0'],r['p65'],r['sv'],r['L1'],r['fv'],r['hdr'][:8].hex(),r['P'][56:70].hex()))
