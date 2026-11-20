#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""steals.py TRACE... - from an EMU8000 port stream (cap.sh): note starts (DCYSUSV write, bit 15 and 7 clear),
note-offs (bit 15 set, no note start right after) and steals (a note set up on a voice whose last note had no note-off), voices in use."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location('es', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'emustream.py'))
for f in sys.argv[1:]:
    sys.argv = ['x', 'none']
    es = importlib.util.module_from_spec(spec)
    try: spec.loader.exec_module(es)
    except Exception: pass
    ev = [(t, int(nm[8:-1]), v) for t, k, nm, v in es.decoded(es.load(f)) if k == 'W' and nm.startswith('DCYSUSV[')]
    on = {}; starts = rel = steals = 0; used = set(); last_rel = {}
    hz = 400e6
    for i, (t, ch, v) in enumerate(ev):
        if v & 0x8000:
            nxt = next((x for x in ev[i + 1:i + 40] if x[1] == ch), None)
            if nxt and not nxt[2] & 0x8000 and (nxt[0] - t) < hz * 0.002:   # set-up of the next note: a steal if still on
                if on.get(ch): steals += 1
            else:
                rel += 1
            on[ch] = False
        elif not v & 0x80:
            starts += 1; used.add(ch); on[ch] = True
    print('%s: %d starts, %d releases, %d steals (%.1f%%), %d voices used' % (f, starts, rel, steals, 100.0 * steals / max(starts, 1), len(used)))
