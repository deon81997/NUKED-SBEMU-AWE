#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""midicap.py - USB MIDI byte-exact check (beta.121).
   midicap.py CAPTURE [FAKEMIDI_LOG] [--quiet]
   CAPTURE: SBDIAG MIDICAP's file, or a raw guest memory dump (QEMU pmemsave) - both hold SBEMU /MIDICAP:1's 4 KB
            pages ("SBMIDCAP", page number, kind G = what the game wrote to the MPU-401 / U = USB MIDI packets sent).
   The game's writes go through a reference MPU-401 model written from the MPU-401 + MIDI 1.0 specs (UART mode, or
   intelligent mode: D0-D7 one message, DF a SysEx, E0-EF one parameter byte, other data = sequencer data, not
   MIDI), then both sides are cut into MIDI events (running status expanded, a real-time byte is its own event placed
   where a MIDI parser sees it, a SysEx is one event). PASS = the device's events (fakemidi log, else the U pages) are
   exactly the expected ones, apart from a tail still queued when the dump was taken (at most QUEUE_MAX events).
   Exit 0 PASS, 1 FAIL, 2 no capture."""
import os, sys, struct

QUEUE_MAX = 1100            # SBEMU's queue (1024 packets) + one transfer in flight
CNT = [0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1]

def pages(data):
    g, u = {}, {}
    for off in range(0, len(data) - 4095, 4096):
        if data[off:off + 8] != b'SBMIDCAP': continue
        n, kind, used = struct.unpack_from('<I', data, off + 8)[0], data[off + 12], struct.unpack_from('<I', data, off + 16)[0]
        if not used or used > 4096 - 20: continue
        (g if kind == ord('G') else u).setdefault(n, data[off + 20:off + 20 + used])
    return g, u

def joined(p):
    out = b''
    for i in range(len(p)):
        if i not in p: break
        out += p[i]
    return out

def model(writes, slots=None):
    """reference MPU-401: game writes (port 0 data / 1 command, value) -> bytes on the MIDI cable.
       beta.123 records: port 2 = output switch (0 emulated synth, 1.. USB slot), port 3 = a byte SBEMU itself sent
       to the USB cable (silence for the old output / state replay for the new one). slots: dict slot -> byte list"""
    out, uart, intel, want, need, rs, ign = [], False, False, 0, -1, None, 0
    cur = 1                                     # captures before beta.123: always USB slot 1
    if slots is None: slots = {}
    def emit(b): out.append(b); slots.setdefault(cur, []).append(b); allm.append(b)
    allm, rep, snap = [], None, None           # all the game's MIDI (either output); replay after a switch to USB
    def progs(bs):
        p = {}
        for e in events(bs):
            if e[0] == 'M' and 0xC0 <= e[1] < 0xD0: p[e[1] & 15] = e[2]
        return p
    def check():
        if rep is None: return
        got = progs(rep)
        bad = [ch for ch in snap if got.get(ch) != snap[ch]]
        REPLAY.append((len(snap), bad))
    for port, v in writes:
        if port == 2:
            check(); cur = v; rep = [] if v else None; snap = progs(allm) if v else None; continue
        if port == 3:
            if cur: slots.setdefault(cur, []).append(v)
            if rep is not None: rep.append(v)
            continue
        if rep is not None: check(); rep = None
        if cur == 0 and port == 0: allm.append(v)
        if cur == 0:                            # the emulated synth has it: the USB side only follows FF / 3F
            if port == 1 and v == 0xFF: uart = intel = False; want = 0
            elif port == 1 and v == 0x3F and not uart: uart = True; want = 0
            continue
        if port:
            if v == 0xFF: uart = intel = False; want = 0; continue
            if uart: continue
            if v == 0x3F: uart = True; want = 0; continue
            if 0xA0 <= v <= 0xBF: continue                  # requests (answered) / SBEMU's own tools
            intel = True
            if 0xD0 <= v <= 0xD7: want, need = 1, -1
            elif v == 0xDF: want = 2
            elif 0xE0 <= v <= 0xEF: want = 3
            else: want = 0
            continue
        if uart or not intel: emit(v); continue
        if want == 1:
            if v >= 0xF8: emit(v); continue
            if need < 0:
                if v >= 0x80:
                    want = 0
                    if v >= 0xF0: ign += 1; continue
                    rs = v; need = 1 if (v & 0xE0) == 0xC0 else 2; emit(v); want = 1; continue
                if rs is None: want = 0; ign += 1; continue
                need = 1 if (rs & 0xE0) == 0xC0 else 2; emit(rs)
            emit(v); need -= 1
            if need <= 0: want = 0
        elif want == 2:
            emit(v)
            if v == 0xF7: want = 0
        elif want == 3: want = 0
        else: ign += 1
    check()
    return out, ign
REPLAY = []                                     # per switch to USB: (channels with a program, channels the replay got wrong)

def events(bs):
    ev, rs, buf, need, sx = [], None, [], 0, None
    for b in bs:
        if b >= 0xF8: ev.append(('RT', b)); continue
        if b == 0xF0:
            if sx is not None: ev.append(('SX',) + tuple(sx) + ('noF7',))
            sx = [b]; rs = None; buf = []; continue
        if sx is not None:
            if b < 0x80: sx.append(b); continue
            if b == 0xF7: sx.append(b); ev.append(('SX',) + tuple(sx)); sx = None; continue
            ev.append(('SX',) + tuple(sx) + ('cut',)); sx = None
        if b == 0xF7: continue
        if b >= 0x80:
            buf = []
            if b < 0xF0: rs, need = b, (1 if (b & 0xE0) == 0xC0 else 2)
            elif b in (0xF1, 0xF3): rs, need = b, 1
            elif b == 0xF2: rs, need = b, 2
            else:
                rs = None
                if b == 0xF6: ev.append(('SC', b))
            continue
        if rs is None: continue
        buf.append(b)
        if len(buf) == need:
            ev.append(('M', rs) + tuple(buf)); buf = []
            if rs >= 0xF0: rs = None
    if sx is not None: ev.append(('SX',) + tuple(sx) + ('open',))
    return ev

def fake_bytes(path):
    out = []
    for l in open(path, errors='replace'):
        i = l.find('MIDI ')
        if i >= 0 and 'MIDI IN' not in l: out += [int(x, 16) for x in l[i + 5:].split()]
    return out

def packet_bytes(u, slot=None):
    out = []
    for i in range(0, len(u) - 3, 4):
        if slot is not None and (u[i] >> 4) + 1 != slot: continue     # beta.123: the slot is in the cable nibble
        out += list(u[i + 1:i + 1 + CNT[u[i] & 15]])
    return out

def fmt(e): return ' '.join('%02X' % x if isinstance(x, int) else x for x in e)

def compare(name, got, exp, quiet):
    n = min(len(got), len(exp))
    for i in range(n):
        if got[i] != exp[i]:
            print('  %s: FIRST DIFFERENCE at event %d: device %s / expected %s' % (name, i, fmt(got[i]), fmt(exp[i])))
            for j in range(max(0, i - 3), min(n, i + 4)): print('    %5d %-40s %s' % (j, fmt(got[j])[:40], fmt(exp[j])[:40]))
            return False
    if len(got) > len(exp): print('  %s: device got %d events MORE than expected' % (name, len(got) - len(exp))); return False
    if len(exp) - len(got) > QUEUE_MAX: print('  %s: %d events never reached the device' % (name, len(exp) - len(got))); return False
    if not quiet: print('  %s: %d events identical (%d still queued at the dump)' % (name, len(got), len(exp) - len(got)))
    return True

def main():
    a = [x for i, x in enumerate(sys.argv) if i and not x.startswith('--') and sys.argv[i - 1] != '--script']
    quiet = '--quiet' in sys.argv
    if '--script' in sys.argv:          # writes from an MPUPLAY script (no capture: e.g. an older SBEMU), no U pages
        sc = open(sys.argv[sys.argv.index('--script') + 1]).read().split()
        writes = [(1 if t[0] == 'C' else 0, int(t[1:], 16)) for t in sc if t[0] in 'CD']
        g, u, ub = {0: b''}, {}, b''
    else:
        data = open(a[0], 'rb').read()
        g, u = pages(data)
        if not g: print('midicap: no SBMIDCAP pages in %s FAIL' % a[0]); return 2
        gb, ub = joined(g), joined(u)
        writes = [(gb[i], gb[i + 1]) for i in range(0, len(gb) - 1, 2)]
    slots = {}
    cable, ign = model(writes, slots)
    if not slots: slots = {1: []}
    nsw = sum(1 for p, v in writes if p == 2)
    kinds = {}
    for e in events(cable): kinds[e[0]] = kinds.get(e[0], 0) + 1
    full = len(g) >= 48 or len(u) >= 32
    print('capture: %d MPU writes (%d data, %d commands), %d expected events %s, %d intelligent-mode bytes not MIDI, %d USB packets, %d output switches%s'
          % (len(writes), sum(1 for p, v in writes if p == 0), sum(1 for p, v in writes if p == 1), len(events(cable)),
             ' '.join('%s=%d' % kv for kv in sorted(kinds.items())), ign, len(ub) // 4, nsw, ' (capture full)' if full else ''))
    ok = True
    for n, bad in REPLAY:
        print('  switch to USB: replay restores %d channel programs%s' % (n, ', WRONG on channels %s' % bad if bad else ''))
        ok = ok and not bad
    logs = a[1:]
    for k in sorted(slots):
        exp = events(slots[k])
        if ub: ok = compare('USB %d: SBEMU sent (U pages)' % k, events(packet_bytes(ub, k if nsw else None)), exp, quiet) and ok
        if logs:
            best, bestn = None, -1
            for l in logs:                       # the fake log that belongs to this slot: the longest common start
                dev = events(fake_bytes(l))
                if full: dev = dev[:len(exp)]
                n = 0
                while n < min(len(dev), len(exp)) and dev[n] == exp[n]: n += 1
                if dev and n > bestn: best, bestn = (l, dev), n
            if best is None and len(logs) == 1 and len(slots) == 1: best = (logs[0], events(fake_bytes(logs[0])))
            if best is None: print('  USB %d: no device log matches (%d events expected)' % (k, len(exp))); ok = ok and not exp
            else: ok = compare('USB %d: device received (%s)' % (k, os.path.basename(best[0])), best[1], exp, quiet) and ok
    print('midicap: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
