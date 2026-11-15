#!/usr/bin/env python3
# mkrmbin.py IN.TXT OUT.BIN - an MPUPLAY script (D/C/A/W tokens) as RMPLAY.COM's RM.BIN (type, value byte pairs;
# W > 255 ms split into several waits). Used for gamecap.sh rm (beta.126-awe.1).
import sys
o = bytearray()
for t in open(sys.argv[1]).read().split():
    k, v = t[0], t[1:]
    if k == 'D': o += bytes((0, int(v, 16)))
    elif k == 'C': o += bytes((1, int(v, 16)))
    elif k == 'A': o += bytes((2, 0))
    elif k == 'W':
        ms = int(v)
        while ms > 0: o += bytes((3, min(ms, 255))); ms -= 255
open(sys.argv[2], 'wb').write(o[:60000])
print(len(o) // 2, 'records', '(cut to 30000)' if len(o) > 60000 else '')
