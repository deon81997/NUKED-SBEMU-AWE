#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""loudcheck.py CAPTURE [--min-gap N] - beta.128: no loud events when the MIDI output is switched.
   From a /MIDICAP:1 capture (rig.py --dump) it rebuilds what EACH output got - the emulated synth (game bytes while
   it was chosen + capture port 4 = SBEMU's own bytes to it) and every USB slot (game bytes + port 3) - runs a MIDI
   state model per output and one for the game's own stream, and checks at every switch:
     old output: after SBEMU's silence no note is held, no SysEx left open;
     new output: every note-on the game sends after a switch finds that output's channel in the game's state
                 (program, bank, volume 7, pan 10, expression 11, pitch bend range) - else it is a "loud event";
     reset gap:  after a replayed GS/GM/XG reset at least N (default 150, ~48 ms) timing-clock bytes before the next
                 message. The output models are deaf for N bytes after a replayed reset, as a real SC-55 is for ~50 ms
                 (it loses what comes sooner): a replay without the gap loses its volume / expression = LOUD events.
   UART-mode captures only (MPUPLAY scripts, MIDPLAY). Last line PASS/FAIL."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from midicap import pages, joined
KEYS = ('prog', 0, 32, 7, 10, 11, 'sens')
def is_reset(m):
    return (len(m) >= 6 and m[1] == 0x7E and m[3] == 9 and m[4] in (1, 3)) or \
           (len(m) >= 11 and m[1] == 0x41 and m[3] == 0x42 and m[4] == 0x12 and m[5:8] == [0x40, 0, 0x7F]) or \
           (len(m) >= 9 and m[1] == 0x43 and m[3] == 0x4C and m[4:7] == [0, 0, 0x7E])
class Model:
    def __init__(s, known=False):
        s.ch = [dict.fromkeys(KEYS) for _ in range(16)]; s.held = [set() for _ in range(16)]
        s.rpn = [[127, 127] for _ in range(16)]; s.rs = None; s.d = []; s.sx = None; s.deaf = 0
        if known: s.reset()
    def reset(s):
        for c in s.ch: c.update({'prog': 0, 0: 0, 32: 0, 7: 100, 10: 64, 11: 127, 'sens': 2})
    def byte(s, b):
        """returns ('on', ch, note) for a note-on, ('sx', bytes) for a whole SysEx, else None"""
        if s.deaf: s.deaf -= 1; return None
        if b >= 0xF8: return None
        if b == 0xF0: s.sx = [b]; s.rs = None; return None
        if s.sx is not None:
            if b < 0x80: s.sx.append(b); return None
            m = s.sx + [0xF7]; s.sx = None
            if is_reset(m): s.reset(); [h.clear() for h in s.held]
            if b == 0xF7: return ('sx', m)
        if b >= 0x80:
            s.d = []; s.rs = b if b < 0xF0 else None; return None
        if s.rs is None: return None
        s.d.append(b); n = 1 if (s.rs & 0xE0) == 0xC0 else 2
        if len(s.d) < n: return None
        st, d = s.rs, s.d; s.d = []; ch, t = st & 15, st & 0xF0; c = s.ch[ch]
        if t == 0x90 and d[1]: s.held[ch].add(d[0]); return ('on', ch, d[0])
        if t in (0x80, 0x90): s.held[ch].discard(d[0])
        elif t == 0xC0: c['prog'] = d[0]
        elif t == 0xB0:
            k, v = d
            if k in (120, 123): s.held[ch].clear()
            elif k == 121: c[11] = 127; s.rpn[ch] = [127, 127]
            elif k in (101, 100): s.rpn[ch][k == 100] = v
            elif k == 6 and s.rpn[ch] == [0, 0]: c['sens'] = v
            elif k in c: c[k] = v
        return None
def main():
    gap_min = int(sys.argv[sys.argv.index('--min-gap') + 1]) if '--min-gap' in sys.argv else 150
    g, _ = pages(open(sys.argv[1], 'rb').read())
    if not g: print('loudcheck: no capture FAIL'); return 2
    gb = joined(g); w = [(gb[i], gb[i + 1]) for i in range(0, len(gb) - 1, 2)]
    game, outs, cur = Model(), {}, None
    def out(o): return outs.setdefault(o, Model())
    loud, hang, gaps, nsw, pend, notes = [], [], [], 0, None, 0
    ownrun = {}                         # output -> [in a replayed reset gap?, F8 count]
    def own(o, b):
        r = ownrun.setdefault(o, [False, 0])
        e = out(o).byte(b)
        if b == 0xF8 and r[0]: r[1] += 1
        elif b < 0xF8 and r[0] and (b >= 0x80 or out(o).sx is None): gaps.append((o, r[1])); r[0] = False
        if e and e[0] == 'sx' and is_reset(e[1]): r[0] = True; r[1] = 0; out(o).deaf = gap_min
    # SBEMU writes the old output's silence (port 3/4 bytes) BEFORE the switch record and the replay after it
    for port, v in w:
        if port == 2:
            nsw += 1
            if cur is not None:
                m = out(cur); h = sum(len(x) for x in m.held)
                if h or m.sx is not None: hang.append((nsw, cur, h, m.sx is not None))
            cur = v; continue
        if port == 1: continue
        if cur is None: cur = 0
        if port in (3, 4): own(cur, v); continue
        game.byte(v); e = out(cur).byte(v)
        if e and e[0] == 'on':
            notes += 1; ch = e[1]
            bad = [k for k in KEYS if game.ch[ch][k] is not None and out(cur).ch[ch][k] != game.ch[ch][k]]
            if bad and nsw: loud.append((nsw, cur, ch, e[2], ', '.join('%s %s!=%s' % (k, out(cur).ch[ch][k], game.ch[ch][k]) for k in bad)))
    print('loudcheck: %d writes, %d switches, %d note-ons checked, outputs %s' % (len(w), nsw, notes, sorted(outs)))
    for x in loud[:12]: print('  LOUD after switch %d on output %d: ch %d note %d (%s)' % x)
    if len(loud) > 12: print('  ... %d loud events in all' % len(loud))
    for x in hang: print('  HANGING on old output after switch %d: output %d, %d notes held, SysEx open %s' % x)
    for o, n in gaps:
        if n < gap_min: print('  RESET GAP too short on output %d: %d F8 bytes (< %d)' % (o, n, gap_min))
    print('  replayed resets: %d, gap F8 bytes min %s' % (len(gaps), min([n for _, n in gaps]) if gaps else '-'))
    ok = nsw > 0 and not loud and not hang and all(n >= gap_min for _, n in gaps)
    print('loudcheck: %s' % ('PASS' if ok else 'FAIL')); return 0 if ok else 1
if __name__ == '__main__': sys.exit(main())
