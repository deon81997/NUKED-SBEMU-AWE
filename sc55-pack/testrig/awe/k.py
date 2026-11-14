# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
# k.py VMDIR shot.png key... : tap keys (hex scancodes, 0x1xx = E0), 200 ms hold, 700 ms apart, then screenshot
import socket, sys, json, os
v = sys.argv[1]; s = socket.socket(socket.AF_UNIX); s.connect(os.path.join(v, 'a.sock')); f = s.makefile('rwb')
def req(c): f.write((c + '\n').encode()); f.flush(); return json.loads(f.readline().decode())
req('hold')
for k in sys.argv[3:]:
    if k.startswith('w'): req('step %d' % int(k[1:])); continue
    req('key down ' + k); req('step 60'); req('key up ' + k); req('step 600')
print(req('screenshot ' + sys.argv[2]))
