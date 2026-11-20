#!/usr/bin/env python3
# recmem.py SOCK OUT.BIN [MB] : copy an AWEREC SBEMU build's port-stream buffer out of a running 86Box guest (automation
# memr, linear view) - for games that never return to DOS (ROTT ends in its credits). Finds the "AWErec" 01 02 header,
# [MB] [VIEW] [MHZ]; writes the AWEREC1 file (same as SBDIAG FLUSH's C:\AWEREC.BIN).
import socket, sys, json, struct
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1]); f = s.makefile('rwb')
def req(c):
    f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
def rd(a, n):
    out = b''
    while n > 0:
        k = min(n, 65536); j = req('memr %#x %d %s' % (a, k, VIEW))
        if not j.get('ok'): raise IOError(j)
        out += bytes.fromhex(j['data']); a += k; n -= k
    return out
VIEW = sys.argv[4] if len(sys.argv) > 4 else 'normal'
req('hold')
mb = int(sys.argv[3]) if len(sys.argv) > 3 else 64
magic = b'AWErec\x01\x02'
for base in range(0, mb << 20, 65536):
    j = req('memr %#x 65536 %s' % (base, VIEW))
    if not j.get('ok'): continue
    i = bytes.fromhex(j['data']).find(magic)
    if i < 0 or i % 4: continue
    a = base + i; hz, n = struct.unpack('<II', rd(a + 8, 8))
    if not (hz < 10**8 and 0 < n <= 2 << 20): continue
    if not hz: hz = int(float(sys.argv[5]) * 1e6 / 256) if len(sys.argv) > 5 else 1562500   # header Hz is 0 if SBEMU had no clock yet: CPU MHz (default 400)
    body = rd(a + 16, n * 8)
    open(sys.argv[2], 'wb').write(b'AWEREC1\0' + struct.pack('<II', hz, n) + body)
    print('found at %#x: %d records' % (a, n)); break
else:
    print('no AWErec buffer found'); sys.exit(1)
req('release')
