#!/usr/bin/env python3
# floptest.py VMDIR IMGDIR : 86Box automation; loads DISKn.IMG whenever UNPAK asks, until TESTDONE.
# GOOD=dir: when a disk is reported damaged, load that disk from GOOD instead (bad-disk + retry test).
import socket, sys, json, os, re, time
v, imgs = sys.argv[1:3]; good = os.environ.get('GOOD')
s = socket.socket(socket.AF_UNIX); s.connect(os.path.join(v, 'a.sock')); f = s.makefile('rwb')
def req(c):
    f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
def scr():
    j = req('text')
    return '\n'.join(bytes.fromhex(r).decode('cp437').rstrip() for r in j['chars']).rstrip() if j.get('ok') else ''
held = req('hold').get('ok'); t = 0; last = None; bad = False
while t < 3600:
    req('step 2000') if held else time.sleep(2); t += 2
    sc = scr(); tail = [l for l in sc.split('\n') if l.strip()][-3:]
    if 'Press F1' in sc: req('key down 0x3b'); req('key up 0x3b')
    m = re.search(r'Insert disk (\d+) in drive A', tail[-1] if tail else '')
    if m and sc != last:
        last = sc
        n = m.group(1); dmg = any('damaged' in l or 'Data error' in l for l in tail)
        src = good if (dmg and good) else imgs; bad |= dmg
        print('   | ' + ' | '.join(tail)); print(f't={t} asks disk {n}{" (reported damaged)" if dmg else ""} -> {src}', req(f'fdd load 0 {src}/DISK{n}.IMG').get('ok'))
        req('step 1500'); req('key down 0x39'); req('step 150'); req('key up 0x39'); req('step 3000'); last = scr()
    if 'TESTDONE' in sc: break
print(f't={t} damaged-seen={bad}'); print(sc)
req('quit')
