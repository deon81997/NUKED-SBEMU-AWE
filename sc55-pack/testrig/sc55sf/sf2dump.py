#!/usr/bin/env python3
"""sf2dump.py FILE.sf2 bank prog - print a preset's instrument zones (generators) of a SoundFont 2."""
import sys, struct
d = open(sys.argv[1], 'rb').read(); ch = {}
def walk(p, end):
    while p + 8 <= end:
        cid = d[p:p + 4]; sz = struct.unpack('<I', d[p + 4:p + 8])[0]
        if cid == b'LIST': walk(p + 12, p + 8 + sz)
        else: ch[cid] = d[p + 8:p + 8 + sz]
        p += 8 + sz + (sz & 1)
walk(12, len(d))
def recs(cid, n, fmt): b = ch[cid]; return [struct.unpack(fmt, b[i:i + n]) for i in range(0, len(b) - n + 1, n)]
phdr = recs(b'phdr', 38, '<20sHHHIII'); pbag = recs(b'pbag', 4, '<HH'); pgen = recs(b'pgen', 4, '<Hh')
inst = recs(b'inst', 22, '<20sH'); ibag = recs(b'ibag', 4, '<HH'); igen = recs(b'igen', 4, '<Hh')
N = {8:'fc',9:'Q',11:'modEnvFc',15:'cho',16:'rev',17:'pan',6:'vibPitch',23:'vibDelay',24:'vibFreq',26:'mAtt',27:'mHold',28:'mDec',29:'mSus',30:'mRel',34:'att',35:'hold',36:'dec',37:'sus',38:'rel',43:'keys',48:'atten',51:'coarse',52:'fine',53:'smp',54:'mode',56:'scale',57:'excl',58:'root'}
bank, prog = int(sys.argv[2]), int(sys.argv[3])
for i in range(len(phdr) - 1):
    if phdr[i][1] == prog and phdr[i][2] == bank:
        for b in range(phdr[i][3], phdr[i + 1][3]):
            for g in range(pbag[b][0], pbag[b + 1][0]):
                if pgen[g][0] == 41:
                    ii = pgen[g][1]; print('inst', inst[ii][0].split(b'\0')[0])
                    for zb in range(inst[ii][1], inst[ii + 1][1]):
                        gs = igen[ibag[zb][0]:ibag[zb + 1][0]]
                        print('  ' + ' '.join('%s=%s' % (N.get(o, o), ('%d-%d' % (v & 255, (v >> 8) & 255)) if o == 43 else v) for o, v in gs))
