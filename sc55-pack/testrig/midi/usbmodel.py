#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""usbmodel.py [--isr HZ] [--buf BYTES] [--tick HZ] MID... - beta.128 timing model of SBEMU /MUSB (USB MIDI OUT on UHCI).
   A game plays the songs through an MPU-401 at its timer rate (--tick, Duke 3D's music task 120 Hz); each MIDI event
   goes into usbpad.c's queue at that moment. The model replays usbpad.c's OUT logic: polls from SBEMU's sound interrupt
   (--isr, the D810 log: 376/s) - collect the transfer in flight, then pace (31250 baud + 128-byte burst, >= 48-byte
   pieces while more waits) and start the next transfer (up to 64 events) - and, for policy 'kick', also from the
   game's MPU-401 status/data accesses (beta.128). UHCI moves 64-byte packets at 1 ms frames; the device (USB Uno)
   takes a packet when its UART buffer (--buf wire bytes) has room and drains it at 31250 baud.
   Reference = a real MPU-401 + DIN cable: every byte on the wire as soon as the wire is free.
   Prints per policy: events, extra delay vs. the cable p50/p99/max, events > 5/10 ms late, longest wire idle while
   MIDI waited, transfers/s."""
import sys, random, mido

CNT = {0x8: 3, 0x9: 3, 0xA: 3, 0xB: 3, 0xC: 2, 0xD: 2, 0xE: 3}
BPS = 3125.0

def events(path, tick):
    """(time, wire bytes) per MIDI event, times quantized to the game's music task; song start = all-channel resets"""
    ev = [(0.0, 3)] * 48                              # Apogee's MIDI_Reset-style CC/program per channel at song start
    t = 0.0
    for m in mido.MidiFile(path):
        t += m.time
        if m.is_meta: continue
        if m.type == 'sysex': ev.append((t, len(m.data) + 2)); continue
        b = m.bytes()
        ev.append((t, CNT.get(b[0] >> 4, len(b))))
    if t > 150: ev = [e for e in ev if e[0] <= 150]   # first 150 s of each song
    return [(int(x * tick + 1) / tick, n) for x, n in ev]

def cable(ev):
    out, w = [], 0.0
    for t, n in ev:
        w = max(w, t) + n / BPS
        out.append(w)
    return out

def host(ev, isr, buf, policy, seed=1):
    """usbpad.c: queue -> paced OUT transfers (<= 64 events) on UHCI -> device buffer -> wire"""
    rnd = random.Random(seed)
    P = 1.0 / isr
    nxt_isr = rnd.random() * P
    q = []                                  # [enqueue time, wire bytes, event index]
    inflight = []                           # packets of the transfer in flight: lists of events (16 per 64-byte packet)
    busy = False; done_at = None
    tok, otime = 128, None
    wire_free, dev_bytes = 0.0, []          # device buffer: list of (event idx, bytes) not yet on the wire
    out = [0.0] * len(ev)
    tx = 0; i = 0; t = 0.0
    idle_max = 0.0; idle_since = None
    end = ev[-1][0] + 5

    def drain(now):
        nonlocal wire_free
        while dev_bytes:
            k, n, ta = dev_bytes[0]
            st = max(wire_free, ta)
            if st + n / BPS > now: break
            wire_free = st + n / BPS; out[k] = wire_free; dev_bytes.pop(0)

    def room(now):
        drain(now)
        used = sum(x[1] for x in dev_bytes)
        return buf - used

    def poll(now):
        nonlocal busy, tok, otime, tx
        if busy:
            if inflight and policy != 'append': return   # not all packets taken yet (NAK): the next poll looks again
            busy = False
        if not q: return
        n = min(len(q), 64)
        R, B, C = (BPS, 128, 48) if policy != 'fix' else (BPS * 1.25, 256, 0)
        if otime is None: otime, tok = now, B
        else:
            add = int((now - otime) * R)
            if add: otime += add / R; tok = min(B, tok + add)
        j = w = 0
        while j < n and (policy == 'nopace' or w + q[j][1] <= tok): w += q[j][1]; j += 1
        if not j or (j < n and w < C): return
        if policy == 'nopace': j, w = n, 0
        tok -= w
        pk = q[:j]; del q[:j]
        for a in range(0, len(pk), 16): inflight.append(pk[a:a + 16])
        busy = True; tx += 1

    wi = 0
    step = 0.0001
    while t < end:
        # game writes due
        while wi < len(ev) and ev[wi][0] <= t:
            q.append((ev[wi][0], ev[wi][1], wi)); wi += 1
            if policy == 'kick': poll(t)
        if t >= nxt_isr: poll(t); nxt_isr += P
        # UHCI frame: packets taken depth first while the device has room
        if abs(t * 1000 - round(t * 1000)) < step * 500:
            while inflight:
                nb = sum(e[1] for e in inflight[0])
                r = room(t)
                if r < nb and (dev_bytes or buf): break     # buf 0: one packet at a time, taken when the wire is free
                for e in inflight.pop(0): dev_bytes.append((e[2], e[1], t))
        drain(t)
        waiting = bool(q or inflight)
        if waiting and not dev_bytes and wire_free <= t:
            if idle_since is None: idle_since = t
        elif idle_since is not None:
            idle_max = max(idle_max, t - idle_since); idle_since = None
        t += step
        if wi >= len(ev) and not q and not inflight and not dev_bytes: break
    return out, tx, idle_max

def pct(v, p): v = sorted(v); return v[min(len(v) - 1, int(len(v) * p))]

def main():
    a = sys.argv[1:]
    isr = float(a[a.index('--isr') + 1]) if '--isr' in a else 376
    buf = int(a[a.index('--buf') + 1]) if '--buf' in a else 0
    tick = float(a[a.index('--tick') + 1]) if '--tick' in a else 120
    pol = a[a.index('--policy') + 1].split(',') if '--policy' in a else ['isr', 'kick']
    files = [x for x in a if x.lower().endswith('.mid')]
    for policy in pol:
        ex, tot, txs, dur, idle = [], 0, 0, 0.0, 0.0
        for f in files:
            ev = events(f, tick)
            ref = cable(ev)
            o, tx, im = host(ev, isr, buf, policy)
            ex += [(o[k] - ref[k]) * 1000 for k in range(len(ev)) if '--skipstart' not in a or ev[k][0] > 1.0]
            tot += len(ev); txs += tx; dur += ev[-1][0]; idle = max(idle, im)
        print('%-4s isr=%d/s buf=%d: events=%d extra p50=%.1fms p99=%.1fms max=%.1fms >5ms=%d >10ms=%d wire-idle-max=%.1fms tx/s=%.1f' % (
            policy, isr, buf, tot, pct(ex, .5), pct(ex, .99), max(ex), sum(e > 5 for e in ex), sum(e > 10 for e in ex), idle * 1000, txs / dur))

if __name__ == '__main__':
    main()
