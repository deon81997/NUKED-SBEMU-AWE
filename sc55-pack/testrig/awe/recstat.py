#!/usr/bin/env python3
# recstat.py AWEREC.BIN : records, time span, writes/reads, register histogram of writes (r,channel-group)
import sys, struct, collections
d = open(sys.argv[1], 'rb').read(); hz, n = struct.unpack_from('<II', d, 8)
recs = [struct.unpack_from('<IHH', d, 16 + 8 * i) for i in range(n)]
t0 = recs[0][0]; span = ((recs[-1][0] - t0) & 0xFFFFFFFF) / hz
w = [r for r in recs if r[1] & 0x8000]; rd = len(recs) - len(w)
print('records %d (%.1f s, %d Hz/256) writes %d reads %d' % (n, span, hz, len(w), rd))
ptr = 0; h = collections.Counter(); rh = collections.Counter()
names = {(0,0):'CPF',(0,1):'PTRX',(0,2):'CVCF',(0,3):'VTFT',(0,4):'Z2',(0,5):'Z1',(0,6):'PSST',(0,7):'CSL',(1,0):'CCCA',(1,1):'cfg1',
 (1,2):'INIT1',(1,3):'INIT3',(1,4):'ENVVOL',(1,5):'DCYSUSV',(1,6):'ENVVAL',(1,7):'DCYSUS',(2,0):'CCCAhi',(2,1):'cfg2',(2,2):'INIT2',(2,3):'INIT4',
 (2,4):'ATKHLDV',(2,5):'LFO1VAL',(2,6):'ATKHLD',(2,7):'LFO2VAL',(3,0):'IP',(3,1):'IFATN',(3,2):'PEFE',(3,3):'FMMOD',(3,4):'TREMFRQ',(3,5):'FM2FRQ2',(3,6):'r3/6',(3,7):'ID'}
for t, off, val in recs:
    o = off & 0x7FFF
    if o == 0x802:
        if off & 0x8000: ptr = val
        continue
    port = {0: 0, 2: 0, 0x400: 1, 0x402: 2, 0x800: 3}[o]
    r = (ptr >> 5) & 7; c = ptr & 31
    k = names.get((port, r), '?')
    if k in ('cfg1', 'cfg2'): k += '/%d' % c
    (h if off & 0x8000 else rh)[k] += 1
print('writes:', ' '.join('%s=%d' % kv for kv in h.most_common(40)))
print('reads:', ' '.join('%s=%d' % kv for kv in rh.most_common(12)))
