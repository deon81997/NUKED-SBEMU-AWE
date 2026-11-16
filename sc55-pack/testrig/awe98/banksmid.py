#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# banksmid.py OUT.MID SBK [REPEAT] - bank manager test song for the AWE32 MIDI Synth: 8 notes each on
# CC0 1 / prog 0 (user bank 1), CC0 2 / SBK's first program (user bank 2), CC0 0 + CC32 2 (bank 2 by LSB),
# CC0 0 (bank 0), CC0 5 (empty: bank 0), drum channel CC0 1 (bank 1's kit). Prints the user-bank notes expected.
import struct, sys
out, sbk = sys.argv[1], sys.argv[2]; rep = int(sys.argv[3]) if len(sys.argv) > 3 else 1
d = open(sbk, 'rb').read(); i = d.index(b'phdr'); p2 = struct.unpack_from('<H', d, i + 8 + 20)[0]
ev = []
def seg(ch, msb, lsb, prog, keys):
    ev.append(bytes([0xB0 | ch, 0, msb])); ev.append(bytes([0xB0 | ch, 32, lsb])); ev.append(bytes([0xC0 | ch, prog]))
    for k in keys: ev.append((bytes([0x90 | ch, k, 100]), 96)); ev.append((bytes([0x80 | ch, k, 0]), 24))
user = 0
for r in range(rep):
    for (ch, msb, lsb, prog, u) in [(0, 1, 0, 0, 1), (0, 2, 0, p2, 1), (0, 0, 2, p2, 1), (0, 0, 0, 0, 0), (0, 5, 0, 0, 0), (9, 1, 0, 0, 1)]:
        seg(ch, msb, lsb, prog, [36, 38, 42, 46, 36, 38, 42, 46] if ch == 9 else [60, 62, 64, 65, 67, 69, 71, 72]); user += 8 * u
trk = b''
for e in ev:
    m, dt = (e, 0) if isinstance(e, bytes) else e
    v = dt; vb = [v & 0x7F]; v >>= 7
    while v: vb.insert(0, (v & 0x7F) | 0x80); v >>= 7
    trk += bytes(vb) + m
trk += b'\x00\xff\x2f\x00'
open(out, 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 96) + b'MTrk' + struct.pack('>I', len(trk)) + trk)
print(f'{out}: program {p2} in bank 2, {user} user-bank notes of {48 * rep}')
