#!/usr/bin/env python3
# drv.py VMDIR SECONDS : boot, record all audio streams for SECONDS of emulated time, screenshots, text screen
import socket, sys, json, os, time
v, secs = sys.argv[1], int(sys.argv[2])
s = socket.socket(socket.AF_UNIX); s.connect(os.path.join(v, 'a.sock')); f = s.makefile('rwb')
def req(c):
    f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
def scr():
    j = req('text')
    if not j.get('ok'): return '(graphics)'
    return '\n'.join(bytes.fromhex(r).decode('cp437').rstrip() for r in j['chars']).rstrip()
od = os.path.join(v, 'aud'); os.makedirs(od, exist_ok=True)
print(req('audio start ' + od))
t = 0
while t < secs:
    st = 1 if t < 20 else 5
    req('step %d' % (st * 1000)) if req('hold').get('ok') else time.sleep(st)
    t += st
    sc = scr()
    if 'Press F1' in sc: req('key down 0x3b'); req('key up 0x3b'); print('(F1)')
    req('screenshot %s/s%03d.png' % (od, t))
    print('--- t=%d' % t); print(sc[-600:] if sc else '')
if os.environ.get('QUIT'):
    def tap(k):
        req('key down %#x' % k); req('step 150'); req('key up %#x' % k); req('step 1500')
    tap(0x01); tap(0x148); tap(0x1c); tap(0x15); req('step 6000'); tap(0x15); req('step 8000')
    print('quit sent'); print(scr()[-300:])
print(json.dumps(req('audio stop'))[:2000])
print(json.dumps(req('info'))[:800])
