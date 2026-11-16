#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# songbank.py IN.MID OUT.MID BANK - the song on user bank BANK: CC0 = BANK at the start of every melodic channel
# and every CC0 in the song set to BANK (CPU test of the AWE32 MIDI Synth playing RAM samples). Needs mido.
import sys, mido
src, dst, bank = sys.argv[1], sys.argv[2], int(sys.argv[3])
m = mido.MidiFile(src)
for t in m.tracks:
    for e in t:
        if e.type == 'control_change' and e.control in (0, 32): e.value = bank if e.control == 0 else 0
m.tracks[0][0:0] = [mido.Message('control_change', channel=c, control=0, value=bank, time=0) for c in range(16) if c != 9]
m.save(dst)
print(dst, 'bank', bank)
