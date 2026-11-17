#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE test rig)
"""probe.py mid OUT.mid [bank]         - test song: every program (bank N, ch 1) C4 vel 100 1.5 s + 1 s gap, then drum
                                          notes 35-81 on ch 10 (0.6 s each), CC91/93 = 0 (dry), GS reset first.
   probe.py cmp REF.wav TEST.wav [bank] - per instrument: envelope dB error (20 ms RMS, gain-matched over the whole
                                          song), octave-band dB error, pitch error (cents), level (dB); worst list."""
import sys, struct, wave, numpy as np
T_ON, T_GAP, T_DR, T0 = 1.5, 1.0, 0.6, 1.0

def vlq(n):
    b = [n & 127]; n >>= 7
    while n: b.insert(0, (n & 127) | 128); n >>= 7
    return bytes(b)

def mkmid(path, bank=0):
    tpq, ev = 480, []  # tempo 500000 -> 960 ticks/s
    t = lambda s: int(round(s * 960))
    ev.append((0, bytes([0xF0, 0x0A, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7])))
    for ch in (0, 9):
        ev += [(t(0.5), bytes([0xB0 | ch, 91, 0])), (t(0.5), bytes([0xB0 | ch, 93, 0])),
               (t(0.5), bytes([0xB0 | ch, 0, bank if ch == 0 else 0]))]
    s = T0
    for p in range(128):
        ev += [(t(s - 0.1), bytes([0xC0, p])), (t(s), bytes([0x90, 60, 100])), (t(s + T_ON), bytes([0x80, 60, 0]))]
        s += T_ON + T_GAP
    for n in range(35, 82):
        ev += [(t(s), bytes([0x99, n, 100])), (t(s + 0.1), bytes([0x89, n, 0]))]; s += T_DR
    ev.sort(key=lambda e: e[0])
    trk, last = b'', 0
    for tk, d in ev:
        trk += vlq(tk - last)
        trk += (bytes([0xF0]) + vlq(len(d) - 1) + d[1:]) if d[0] == 0xF0 else d
        last = tk
    trk += vlq(t(2)) + b'\xff\x2f\x00'
    open(path, 'wb').write(b'MThd' + struct.pack('>IHHH', 6, 0, 1, tpq) + b'MTrk' + struct.pack('>I', len(trk)) + trk)

def load(fn):
    w = wave.open(fn); r = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').reshape(-1, w.getnchannels()).astype(float).mean(1) / 32768
    return x, r

def onset(x, r):  # first sample above -50 dBFS
    i = np.argmax(np.abs(x) > 0.003); return i / r

def seg(x, r, a, b):
    return x[int(a * r):int(b * r)]

def env(x, r):
    h = int(r * 0.02); n = len(x) // h
    return 10 * np.log10(np.maximum((x[:n * h].reshape(n, h) ** 2).mean(1), 1e-10))

def bands(x, r):
    sp = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2; f = np.fft.rfftfreq(len(x), 1 / r)
    return np.array([10 * np.log10(sp[(f >= lo) & (f < lo * 2)].sum() + 1e-12) for lo in (62.5, 125, 250, 500, 1000, 2000, 4000, 8000)])

def pitch(x, r):
    x = x[int(len(x) * 0.25):int(len(x) * 0.75)]
    if len(x) < 2048: return None
    N = 1 << int(np.ceil(np.log2(len(x)) + 2)); sp = np.abs(np.fft.rfft(x * np.hanning(len(x)), N)); f = np.fft.rfftfreq(N, 1 / r)
    ok = (f > 40) & (f < 5000); i = np.argmax(sp * ok)
    return f[i] if sp[i] > 1e-3 * sp.max() else None

def cmp(ref, test):
    X, rx = load(ref); Y, ry = load(test)
    ox, oy = onset(X, rx), onset(Y, ry)
    items = [('P%03d' % p, T0 + p * (T_ON + T_GAP), T_ON + T_GAP) for p in range(128)]
    s = T0 + 128 * (T_ON + T_GAP)
    items += [('D%03d' % n, s + (n - 35) * T_DR, T_DR) for n in range(35, 82)]
    first = items[0][1]; rows = []
    for name, st, dur in items:
        a = seg(X, rx, ox + st - first, ox + st - first + dur); b = seg(Y, ry, oy + st - first, oy + st - first + dur)
        if len(a) < rx * dur * 0.9 or len(b) < ry * dur * 0.9: continue
        ea, eb = env(a, rx), env(b, ry); n = min(len(ea), len(eb)); ea, eb = ea[:n], eb[:n]
        loud = ea > ea.max() - 40
        lvl_a, lvl_b = ea.max(), eb.max()
        ba, bb = bands(a, rx), bands(b, ry)
        pa, pb = pitch(a, rx), pitch(b, ry)
        cents = 1200 * np.log2(pb / pa) if pa and pb else float('nan')
        rows.append([name, lvl_a, lvl_b, ea, eb, loud, ba - ba.max(), bb - bb.max(), cents])
    g = np.median([r[1] - r[2] for r in rows if r[1] > -60])  # gain match
    out = []
    for name, la, lb, ea, eb, loud, ba, bb, c in rows:
        ee = np.abs(ea - (eb + g))[loud].mean() if loud.any() else 0
        be = np.abs(ba - bb)[ba > -40].mean()
        out.append((ee + 0.5 * be + (min(abs(c), 1200) / 100 if c == c else 0), name, la, lb + g, ee, be, c))
    print('gain TEST->REF %+.1f dB; %d items; mean env err %.2f dB, band err %.2f dB, |pitch| med %.1f c' % (
        g, len(out), np.mean([o[4] for o in out]), np.mean([o[5] for o in out]),
        np.nanmedian([abs(o[6]) for o in out])))
    for o in sorted(out, reverse=True)[:int(sys.argv[5]) if len(sys.argv) > 5 else 10]:
        print('%s score %5.2f  lvl ref %6.1f test %6.1f  env %5.2f dB  band %5.2f dB  pitch %+7.1f c' % (o[1], o[0], o[2], o[3], o[4], o[5], o[6]))
    return out

if __name__ == '__main__':
    if sys.argv[1] == 'mid': mkmid(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 0)
    else: cmp(sys.argv[2], sys.argv[3])
