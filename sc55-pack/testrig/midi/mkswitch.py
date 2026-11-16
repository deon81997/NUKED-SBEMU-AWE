#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""mkswitch.py OUTDIR - MPUPLAY scripts for the MIDI output switch test (beta.123): SWA = handshake, GM + GS resets,
   a GS SysEx, every channel's bank / program / controllers / pitch bend range / pitch bend, then a song; SWB SWC SWD =
   more of the song only (no set-up), so after each switch the new output must have got the state from SBEMU."""
import os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
R = random.Random(123)
def D(b): return 'D%02X' % b
def roland(addr, data):
    s = sum(addr) + sum(data)
    return [0xF0, 0x41, 0x10, 0x42, 0x12] + addr + data + [(128 - s % 128) % 128, 0xF7]
def song(n):
    t = []
    for i in range(n):
        ch = R.randrange(16); k = R.random()
        if k < 0.6: b = [0x90 | ch, R.randrange(30, 90), R.choice([0, R.randrange(1, 128)])]
        elif k < 0.8: b = [0x80 | ch, R.randrange(30, 90), 64]
        elif k < 0.9: b = [0xB0 | ch, R.choice([1, 7, 10, 11, 64]), R.randrange(128)]
        else: b = [0xE0 | ch, R.randrange(128), R.randrange(128)]
        t += [D(x) for x in b]
        if R.random() < 0.3: t.append('W%d' % R.choice([2, 5, 10]))
    return t
a = ['CFF', 'A', 'C3F', 'A'] + [D(x) for x in [0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7]] + ['W50']
a += [D(x) for x in roland([0x40, 0x00, 0x7F], [0x00])] + ['W50'] + [D(x) for x in roland([0x40, 0x01, 0x30], [0x04])]
for ch in range(16):
    a += [D(x) for x in [0xB0 | ch, 0, R.randrange(4), 32, 0, 0xC0 | ch, R.randrange(128), 0xB0 | ch, 7, R.randrange(60, 127),
                         10, R.randrange(128), 91, R.randrange(128), 101, 0, 100, 0, 6, R.randrange(1, 13), 38, 0, 101, 127, 100, 127,
                         0xE0 | ch, R.randrange(128), R.randrange(128)]]
a += song(400)
os.makedirs(sys.argv[1], exist_ok=True)
for name, t in (('SWA', a), ('SWB', song(400)), ('SWC', song(400)), ('SWD', song(400))):
    open(os.path.join(sys.argv[1], name + '.TXT'), 'w').write(' '.join(t + ['W2000']) + '\r\n')
# SWS (hotkey case, added beta.126-awe.1: switch.sh used it but nothing made it): ~60 s of steady song, an event every
# 20 ms, so Ctrl+Alt+F11 at rig time 45/58/71/84 s always meets MPU-401 writes (the switch waits for the next one)
sw = []
for e in song(3000): sw += [e] if e[0] == 'D' else []
out = []
for i, e in enumerate(sw):
    out.append(e)
    if i % 3 == 2: out.append('W20')
open(os.path.join(sys.argv[1], 'SWS.TXT'), 'w').write(' '.join(out) + '\r\n')
# beta.128 loudcheck: SWQ = a quiet GS set-up (volume 15-40, expression 20-50, pan, bank, program, bend range 2-12,
# part-level DT1s sent again and again + SC-55 display SysEx, which the replay must not let fill its 2 KB), then
# SWL = ~80 s in QEMU (trapped I/O ~40 bytes/s) of held notes and chords on every channel, so Ctrl+Alt+F11 lands mid-note and mid-message.
Q = random.Random(128)
q = ['CFF', 'A', 'C3F', 'A'] + [D(x) for x in roland([0x40, 0x00, 0x7F], [0x00])] + ['W60']
for ch in range(16):
    q += [D(x) for x in [0xB0 | ch, 0, Q.randrange(2), 32, 0, 0xC0 | ch, Q.randrange(128), 0xB0 | ch, 7, Q.randrange(15, 41),
                         10, Q.randrange(128), 11, Q.randrange(20, 51), 101, 0, 100, 0, 6, Q.randrange(2, 13), 38, 0, 101, 127, 100, 127]]
for rep in range(4):
    for part in range(16):
        q += [D(x) for x in roland([0x40, 0x10 | part, 0x19], [Q.randrange(40, 100)])]                       # part level
    q += [D(x) for x in [0xF0, 0x41, 0x10, 0x45, 0x12, 0x10, 0x00, 0x00] + [0x20] * 32 + [0x10, 0xF7]] + ['W5']  # display
open(os.path.join(sys.argv[1], 'SWQ.TXT'), 'w').write(' '.join(q + ['W200']) + '\r\n')
l, on = [], {}
for i in range(2400):
    ch = Q.randrange(16); n = Q.randrange(36, 84)
    if (ch, n) in on: l += [D(0x80 | ch), D(n), D(64)]; del on[(ch, n)]
    else: l += [D(0x90 | ch), D(n), D(Q.randrange(60, 128))]; on[(ch, n)] = 1
    if Q.random() < 0.05: l += [D(0xB0 | ch), D(1), D(Q.randrange(128))]
    l.append('W%d' % Q.choice([5, 10, 20, 40]))
for ch, n in on: l += [D(0x80 | ch), D(n), D(64)]
open(os.path.join(sys.argv[1], 'SWL.TXT'), 'w').write(' '.join(l + ['W500']) + '\r\n')
