#!/usr/bin/env python3
# sends.py AWEREC.BIN : histograms of the per-voice effect sends and pan a driver writes (PTRX bits 15:8 = reverb,
# CSL bits 31:24 = chorus, PSST bits 31:24 = pan) and the FX setup words (cfg/INIT writes to voices 0-31 r1/r2 + 3/6 ...).
import sys, struct, collections
d = open(sys.argv[1], 'rb').read(); hz, n = struct.unpack_from('<II', d, 8)
ptr = 0; H = {k: collections.Counter() for k in ('rev', 'cho', 'pan')}
for i in range(n):
    t, off, val = struct.unpack_from('<IHH', d, 16 + 8 * i)
    if not off & 0x8000: continue
    o = off & 0x7FFF
    if o == 0x802: ptr = val; continue
    r = (ptr >> 5) & 7
    if o == 0 and r == 1: H['rev'][val >> 8] += 1        # PTRX low word
    if o == 2 and r == 7: H['cho'][val >> 8] += 1        # CSL high word
    if o == 2 and r == 6: H['pan'][val >> 8] += 1        # PSST high word
for k, c in H.items(): print(k, ' '.join('%02X:%d' % kv for kv in sorted(c.items())))
