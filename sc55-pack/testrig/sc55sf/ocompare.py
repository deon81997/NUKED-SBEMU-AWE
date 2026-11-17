#!/usr/bin/env python3
"""ocompare.py SF2 [bank] [progs|drum] - per-instrument check of a SoundFont made by SC55SF against Nuked-SC55 itself.
For each program: notes 48/60/72 (vel 100, 1.2 s held, 0.8 s release) rendered dry (CC91/93 = 0) by the oracle and by
TinySoundFont (tsfplay = sbemu/tsf.h, the /VSF engine; RENDER=fluid for fluidsynth); reports level offset, envelope error (dB, 10 ms frames, mean abs over the louder of the two > -60 dB) and
octave-band spectrum error (dB, level-matched). Lower = closer. GPL-2.0-or-later."""
import sys, os, struct, subprocess, wave, numpy as np, json
from concurrent.futures import ThreadPoolExecutor
import oprobe as probe
SF2 = sys.argv[1]; BANK = sys.argv[2] if len(sys.argv) > 2 else '0'
DRUM = BANK == 'drum'
KEYS = [48, 60, 72]; ON, OFF = 1200, 800
DKEYS = [35, 36, 38, 40, 42, 46, 49, 51, 56, 60]
def vlq(n):
    b = [n & 127]; n >>= 7
    while n: b.insert(0, (n & 127) | 128); n >>= 7
    return bytes(b)
def midi(prog, bank, fn, keys):
    ch = 9 if DRUM else 0; ev = b''
    ev += vlq(0) + bytes([0xB0 | ch, 91, 0]) + vlq(0) + bytes([0xB0 | ch, 93, 0])
    if bank and not DRUM: ev += vlq(0) + bytes([0xB0 | ch, 0, bank])
    ev += vlq(0) + bytes([0xC0 | ch, prog]) + vlq(1000) + bytes([0xB0 | ch, 7, 100])
    for k in keys:
        ev += vlq(0) + bytes([0x90 | ch, k, 100]) + vlq(ON) + bytes([0x80 | ch, k, 64]) + vlq(OFF) + bytes([0xB0 | ch, 7, 100])
    ev += vlq(0) + b'\xff\x2f\x00'
    hdr = b'MThd' + struct.pack('>IHHH', 6, 0, 1, 1000)   # 1000 ticks / quarter at 60 bpm = 1 ms
    trk = vlq(0) + b'\xff\x51\x03' + bytes([0x0f, 0x42, 0x40]) + ev
    open(fn, 'wb').write(hdr + b'MTrk' + struct.pack('>I', len(trk)) + trk)
def load(fn):
    w = wave.open(fn); r = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, w.getnchannels()).astype(float).mean(1)
    return x, r
def frames(x, r, t0, dur):   # RMS dB, 10 ms frames
    a = int(t0 * r); n = int(r / 100); m = int(dur * 100)
    seg = x[a:a + n * m]; seg = np.pad(seg, (0, n * m - len(seg)))
    return 10 * np.log10(np.mean(seg.reshape(m, n) ** 2, 1) + 1e-3)
def bands(x, r, t0, dur):
    a = int(t0 * r); seg = x[a:a + int(dur * r)]
    S = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2; f = np.fft.rfftfreq(len(seg), 1 / r)
    edges = [62, 125, 250, 500, 1000, 2000, 4000, 8000, 14000]
    return np.array([10 * np.log10(S[(f >= edges[i]) & (f < edges[i + 1])].sum() + 1e-3) for i in range(len(edges) - 1)])
def onset(x, r, start):
    a = int(start * r); e = np.abs(x[a:a + r]); th = max(e.max() * 0.05, 30)
    idx = np.argmax(e > th); return start + idx / r if e.max() > 30 else None
def one(prog):
    d = '/tmp/sc55sf_cmp_%d' % prog; os.makedirs(d, exist_ok=True)
    keys = DKEYS if DRUM else KEYS
    midi(prog, 0 if DRUM else int(BANK), d + '/t.mid', keys)
    # oracle script from the same events
    ch = 9 if DRUM else 0
    L = ['w 2000', 'wav %s/ref.wav' % d, 'm %x 5b 00' % (0xb0 | ch), 'm %x 5d 00' % (0xb0 | ch)]
    if not DRUM and int(BANK): L.append('m %x 00 %x' % (0xb0 | ch, int(BANK)))
    L += ['m %x %x' % (0xc0 | ch, prog), 'w 1000']
    for k in keys: L += ['m %x %x 64' % (0x90 | ch, k), 'w %d' % ON, 'm %x %x 40' % (0x80 | ch, k), 'w %d' % OFF]
    probe.run(L)
    probe.RUNOK = True
    if os.path.exists(d + '/sf.wav'): os.remove(d + '/sf.wav')
    if os.environ.get('RENDER', 'tsf') == 'fluid':
        subprocess.run(['fluidsynth', '-ni', '-q', '-R', '0', '-C', '0', '-g', '1.0', '-r', '44100', '-F', d + '/sf.wav', SF2, d + '/t.mid'], capture_output=True)
    else: subprocess.run([os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tsfplay'), SF2, d + '/t.mid', d + '/sf.wav'], capture_output=True)
    a, ra = load(d + '/ref.wav'); b, rb = load(d + '/sf.wav')
    res = []
    for i, k in enumerate(keys):
        ta = onset(a, ra, 1.0 - 0.05 + i * (ON + OFF) / 1000); tb = onset(b, rb, 1.0 - 0.05 + i * (ON + OFF) / 1000)
        if ta is None or tb is None: res.append(dict(key=k, missing='ref' if ta is None else 'sf2')); continue
        dur = (ON + OFF) / 1000 - 0.05
        fa, fb = frames(a, ra, ta, dur), frames(b, rb, tb, dur)
        ba, bb = bands(a, ra, ta, min(0.5, dur)), bands(b, rb, tb, min(0.5, dur))
        res.append(dict(key=k, fa=fa.tolist(), fb=fb.tolist(), ba=ba.tolist(), bb=bb.tolist()))
    return prog, res
if __name__ == '__main__':
    progs = [int(x) for x in sys.argv[3:]] or (list(range(128)) if not DRUM else [0, 8, 16, 24, 25, 32, 40, 48, 56, 127])
    with ThreadPoolExecutor(2) as ex: R = list(ex.map(one, progs))
    out = sys.argv[0].replace("ocompare.py", "") + 'cmp_%s.json' % BANK
    json.dump(R, open('/tmp/cmp_%s.json' % BANK, 'w'))
    # summary
    offs = []
    for p, res in R:
        for r in res:
            if 'fa' in r: offs.append(np.median(np.array(r['fb'][:50])) - np.median(np.array(r['fa'][:50])))
    G = float(np.median(offs)); print('global level offset sf2-ref %.1f dB (removed)' % G)
    rows = []
    for p, res in R:
        ee, be, le, miss = [], [], [], 0
        for r in res:
            if 'fa' not in r: miss += 1; continue
            fa, fb = np.array(r['fa']), np.array(r['fb']) - G
            m = np.maximum(fa, fb) > fa.max() - 50
            ee.append(np.mean(np.abs(fa[m] - fb[m]))); le.append(np.max(fb) - np.max(fa))
            ba, bb = np.array(r['ba']), np.array(r['bb']); ba -= ba.max(); bb -= bb.max(); mb = ba > -40
            be.append(np.mean(np.abs(ba[mb] - bb[mb])))
        rows.append((p, np.mean(ee) if ee else 99, np.mean(be) if be else 99, np.mean(le) if le else 0, miss))
    rows.sort(key=lambda r: -(r[1] + r[2] + abs(r[3])))
    for p, e, b, l, miss in rows:
        print('prog %3d env %5.1f dB  bands %5.1f dB  level %+5.1f dB  missing %d' % (p, e, b, l, miss))
    print('mean env %.2f bands %.2f |level| %.2f' % (np.mean([r[1] for r in rows if r[1] < 99]), np.mean([r[2] for r in rows if r[2] < 99]), np.mean([abs(r[3]) for r in rows])))
