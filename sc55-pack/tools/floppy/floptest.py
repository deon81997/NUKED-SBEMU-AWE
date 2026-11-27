#!/usr/bin/env python3
# floptest.py VMDIR IMGDIR : 86Box automation; swaps DISKn.IMG when INSTALL asks, until C:\DONE.TXT
import socket, sys, json, os, re, subprocess
v, imgs = sys.argv[1:3]
s = socket.socket(socket.AF_UNIX); s.connect(os.path.join(v, 'a.sock')); f = s.makefile('rwb')
def req(c):
    f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
def scr():
    j = req('text')
    return '\n'.join(bytes.fromhex(r).decode('cp437').rstrip() for r in j['chars']).rstrip() if j.get('ok') else ''
held = req('hold').get('ok'); done = set(); t = 0
while t < 1800:
    req('step 2000') if held else __import__('time').sleep(2); t += 2
    sc = scr()
    if 'Press F1' in sc: req('key down 0x3b'); req('key up 0x3b')
    m = re.findall(r'Insert disk (\d) in drive A', sc)
    if m and m[-1] not in done:
        n = m[-1]; done.add(n)
        print(f't={t} asked for disk {n}', req('fdd load 0 ' + os.path.join(imgs, f'DISK{n}.IMG')).get('ok'))
        req('step 1500'); req('key down 0x39'); req('step 150'); req('key up 0x39')
    if 'could not be read' in sc and 'R' not in done:
        done.add('R'); good = os.environ.get('GOOD')
        print(f't={t} bad disk reported, inserting good copy', req('fdd load 0 ' + os.path.join(good, 'DISK3.IMG')).get('ok'))
        req('step 1500'); req('key down 0x39'); req('step 150'); req('key up 0x39')
    if 'TESTDONE' in sc: break
print(f't={t}'); print(sc)
req('quit')
