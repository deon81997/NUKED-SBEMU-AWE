#!/usr/bin/env python3
"""SMF -> event file for render.cpp ([u32 frame @32000][u16 len][bytes]).
   smf2ev.py IN.MID OUT.EV        convert a MIDI file
   smf2ev.py --gen OUT.EV [OUT.MID] MT-32 test: reset, reverb modes, all 128 timbres (chord + bend + mod
                                  + volume/pan sweeps), rhythm keys 35-81, a timbre sysex edit; also writes the .MID
"""
import struct, sys

RATE = 32000

def vlq(b, i):
    v = 0
    while True:
        c = b[i]; i += 1; v = (v << 7) | (c & 0x7F)
        if not c & 0x80: return v, i

def parse(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'MThd'
    fmt, ntr, div = struct.unpack('>HHH', b[8:14])
    i = 14; evs = []
    for t in range(ntr):
        assert b[i:i+4] == b'MTrk', 'bad track'
        ln = struct.unpack('>I', b[i+4:i+8])[0]; j = i + 8; end = j + ln; tick = 0; run = 0
        while j < end:
            d, j = vlq(b, j); tick += d; st = b[j]
            if st == 0xFF:
                typ = b[j+1]; l, k = vlq(b, j+2); data = b[k:k+l]; j = k + l
                if typ == 0x51: evs.append((tick, 0, 'tempo', int.from_bytes(data, 'big')))
                continue
            if st in (0xF0, 0xF7):
                l, k = vlq(b, j+1); data = b[k:k+l]; j = k + l
                evs.append((tick, 1, 'raw', (bytes([0xF0]) + data) if st == 0xF0 else data)); continue
            if st & 0x80: run = st; j += 1
            n = 1 if (run & 0xF0) in (0xC0, 0xD0) else 2
            evs.append((tick, 1, 'raw', bytes([run]) + b[j:j+n])); j += n
        i = end
    evs.sort(key=lambda e: (e[0], e[1]))
    out = []; tempo = 500000; last_tick = 0; t_us = 0.0
    for tick, _, kind, v in evs:
        t_us += (tick - last_tick) * tempo / div; last_tick = tick
        if kind == 'tempo': tempo = v; continue
        out.append((int(t_us * RATE / 1e6), v))
    return out

def gen():
    ev = []; t = 0.0
    def at(sec, *bs): ev.append((int(sec * RATE), bytes(bs)))
    def sysex(sec, addr, data):
        body = bytes(addr) + bytes(data); cs = (128 - sum(body) % 128) % 128
        ev.append((int(sec * RATE), bytes([0xF0, 0x41, 0x10, 0x16, 0x12]) + body + bytes([cs, 0xF7])))
    sysex(0.0, [0x7F, 0, 0], [0])                      # MT-32 reset
    t = 0.5
    for mode in range(4):                              # reverb room/hall/plate/tap delay
        sysex(t, [0x10, 0, 1], [mode, 5, 6])
        at(t + .05, 0xC1, mode * 7); at(t + .1, 0x91, 60, 110); at(t + .5, 0x81, 60, 0); t += 1.2
    for prog in range(128):
        ch = 1 + prog % 8                              # parts 2-9 = channels 1..8
        at(t, 0xC0 | ch, prog); at(t, 0xB0 | ch, 7, 100); at(t, 0xB0 | ch, 10, prog % 128)
        for k, n in enumerate((48, 55, 60, 64)): at(t + .01 * k, 0x90 | ch, n, 64 + 15 * k)
        for s in range(8):                             # pitch bend + mod wheel sweep
            v = 8192 + int(6000 * (s / 7.0 - 0.5))
            at(t + .1 + s * .04, 0xE0 | ch, v & 0x7F, v >> 7); at(t + .1 + s * .04, 0xB0 | ch, 1, s * 16)
        at(t + .45, 0xB0 | ch, 7, 60); at(t + .45, 0xE0 | ch, 0, 64); at(t + .45, 0xB0 | ch, 1, 0)
        for n in (48, 55, 60, 64): at(t + .55, 0x80 | ch, n, 0)
        t += 0.62
    for n in range(35, 82):                            # rhythm part, channel 10
        at(t, 0x99, n, 40 + (n * 7) % 88); at(t + .1, 0x89, n, 0); t += 0.16
    sysex(t, [0x08, 0, 0], [ord(c) for c in 'SBEMUTEST '])  # timbre temp name edit, part 1
    sysex(t, [0x08, 0, 0x0E], [1, 3, 4])                   # structure / partial mute bytes
    at(t + .05, 0x91, 60, 100); at(t + .8, 0x81, 60, 0)
    ev.sort(key=lambda e: e[0])
    return ev

def write_ev(evs, path):
    with open(path, 'wb') as f:
        for fr, data in evs: f.write(struct.pack('<IH', fr, len(data)) + data)

def write_mid(evs, path):                              # 1 tick = 1 ms at 120 bpm / 500 ppq... use ppq 500, tempo 500000
    trk = bytearray(); last = 0
    for fr, data in evs:
        tick = int(fr * 1000 / RATE); d = tick - last; last = tick
        v = [d & 0x7F]; d >>= 7
        while d: v.insert(0, 0x80 | (d & 0x7F)); d >>= 7
        trk += bytes(v)
        if data[0] == 0xF0:
            l = len(data) - 1; trk += bytes([0xF0, l]) + data[1:] if l < 128 else b''
        else: trk += data
    trk += b'\x00\xFF\x2F\x00'
    open(path, 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 500) + b'MTrk' + struct.pack('>I', len(trk)) + trk)

if __name__ == '__main__':
    if sys.argv[1] == '--gen':
        e = gen(); write_ev(e, sys.argv[2])
        if len(sys.argv) > 3: write_mid(e, sys.argv[3])
    else: write_ev(parse(sys.argv[1]), sys.argv[2])
