#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# keys.py QMP_SOCK COM2_FILE SECONDS SEED - waits for KEYGO on COM2, then plays Duke-style arrow traffic through QMP
# input-send-event on the PS/2 keyboard: held arrows with typematic repeats (a make every 33 ms), Ctrl / Alt
# (and right Ctrl / AltGr, also E0 keys) pressed and released in the middle, two keys changing at once, quick taps.
# Everything is released before KEYEND. Prints the number of key events sent.
import json, random, socket, sys, time
sock, com2, secs, seed = sys.argv[1], sys.argv[2], float(sys.argv[3]), int(sys.argv[4])
random.seed(seed)
for _ in range(600):
    try:
        if 'KEYGO' in open(com2, errors='replace').read(): break
    except OSError: pass
    time.sleep(0.5)
else: sys.exit('no KEYGO')
s = socket.socket(socket.AF_UNIX); s.connect(sock); f = s.makefile('rw')
def cmd(c):
    f.write(json.dumps(c) + '\n'); f.flush()
    while True:
        r = json.loads(f.readline())
        if 'return' in r or 'error' in r: return r
f.readline(); cmd({'execute': 'qmp_capabilities'})
n = 0
import os
def ev(*keys):   # (qcode, down) pairs in one event batch = "at once"
    global n
    cmd({'execute': 'input-send-event', 'arguments': {'events': [{'type': 'key', 'data': {'down': d, 'key': {'type': 'qcode', 'data': k}}} for k, d in keys]}})
    n += len(keys)
arrows = ['right', 'left', 'up', 'down']; mods = ['ctrl', 'alt', 'ctrl_r', 'alt_r', 'shift']
if os.environ.get('HOTKEY'):   # Ctrl+Alt+F12 twice (overlay on/off): the watcher must log both
    for _ in range(2):
        ev(('ctrl', True)); ev(('alt', True)); ev(('f12', True)); time.sleep(0.1); ev(('f12', False)); ev(('alt', False)); ev(('ctrl', False)); time.sleep(1)
t_end = time.time() + secs
while time.time() < t_end:
    held = ['right'] if random.random() < 0.6 else [random.choice(arrows)]
    if random.random() < 0.3: held.append(random.choice([a for a in arrows if a not in held]))
    ev(*[(k, True) for k in held])
    mod = random.choice(mods) if random.random() < 0.5 else None
    hold = random.uniform(0.2, 1.5); t0 = time.time(); modon = False
    while time.time() - t0 < hold:
        time.sleep(0.033)
        ev(*[(k, True) for k in held])                       # typematic repeat
        if mod and not modon and time.time() - t0 > hold / 3: ev((mod, True)); modon = True
        elif mod and modon and time.time() - t0 > hold * 2 / 3: ev((mod, False)); mod = None
    if mod and modon: ev((mod, False))
    ev(*[(k, False) for k in held])                          # all held keys up at once
    if random.random() < 0.3: ev(('right', True)); ev(('right', False))   # a tap right after
    time.sleep(random.uniform(0.0, 0.15))
print('sent', n, 'key events')
