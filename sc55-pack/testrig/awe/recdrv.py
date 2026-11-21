#!/usr/bin/env python3
# recdrv.py VMDIR SECONDS GAME : (DESCLEVEL=1: Descent level 1 - pilot, New Game, Esc briefing at t=35) boot, record audio for SECONDS of emulated time (attract mode), then the game's quit keys
import socket, sys, json, os, time
v, secs, game = sys.argv[1], int(sys.argv[2]), sys.argv[3]
s = socket.socket(socket.AF_UNIX); s.connect(os.path.join(v, 'a.sock')); f = s.makefile('rwb')
def req(c):
    f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
def scr():
    j = req('text')
    if not j.get('ok'): return '(graphics)'
    return '\n'.join(bytes.fromhex(r).decode('cp437').rstrip() for r in j['chars']).rstrip()
def tap(k, wait=1500, hold=150):
    req('key down %#x' % k); req('step %d' % hold); req('key up %#x' % k); req('step %d' % wait)
od = os.path.join(v, 'aud'); os.makedirs(od, exist_ok=True)
print(req('audio start ' + od))
t = 0
while t < secs:
    st = 1 if t < 20 else 5
    req('step %d' % (st * 1000)) if req('hold').get('ok') else time.sleep(st)
    t += st
    sc = scr()
    if 'Press F1' in sc: tap(0x3b, 100); print('(F1)')
    if game == 'descent' and os.environ.get('DESCLEVEL') and t == 35:   # pilot "A", New Game, difficulty, skip briefing
        for k, w in ((0x1e, 800), (0x1c, 4000), (0x1c, 4000), (0x1c, 4000), (0x1c, 6000), (0x01, 3000), (0x01, 3000)):
            tap(k, w, 300)
        req('screenshot %s/lvl.png' % od); print('(level keys)')
    if t % 10 == 0: req('screenshot %s/s%03d.png' % (od, t))
    print('--- t=%d' % t); print(sc[-300:] if sc else '')
if game == 'duke':
    tap(0x01); tap(0x148); tap(0x1c); tap(0x15); req('step 6000'); tap(0x15); req('step 8000')
elif game == 'rott':                    # attract mode -> menu (Esc), Quit is the last item (Up wraps), Y
    tap(0x01, 3000); req('screenshot %s/q1.png' % od); tap(0x148, 1500); tap(0x1c, 3000); req('screenshot %s/q2.png' % od)
    tap(0x15, 4000, 400); req('screenshot %s/q3.png' % od); tap(0x1c, 6000, 400); tap(0x39, 6000, 400)
elif game == 'descent':                 # title/menu: Esc (quit?), Y
    tap(0x01, 3000); tap(0x01, 3000); tap(0x15, 6000)
else:                                   # Heretic: F10, Y
    tap(0x44, 3000); tap(0x15, 6000); tap(0x15, 6000)
print('quit sent'); req('step 4000'); req('screenshot %s/q4.png' % od); print(scr()[-600:])
print(json.dumps(req('audio stop'))[:500])
