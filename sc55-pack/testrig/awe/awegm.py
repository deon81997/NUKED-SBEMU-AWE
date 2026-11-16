#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""awegm.py AWE32.RAW > awe/synth/awegm.h - NUKED's own GM map over the AWE32 1 MB GM ROM (no Creative bank).
Finds the samples (runs between >40 zero words), their pitch (YIN) and, for sustained ones, a loop at the tail
(best splice in the last part, at least 2 periods), then builds 128 programs + 1 drum kit from the FAM / DRUM
tables below (sample numbers picked by ROM order and sound features: decay, centroid, pitch; see HANDOFF)."""
import sys, numpy as np
rom = np.frombuffer(open(sys.argv[1], 'rb').read(), dtype='<i2').astype(np.float64)
if len(rom) > 524288: rom = rom[:524288]
nz = np.flatnonzero(rom[0x1E0:] != 0) + 0x1E0
segs = []; st = pv = nz[0]
for k in nz[1:]:
    if k - pv > 40: segs.append((int(st), int(pv) + 1)); st = k
    pv = k
segs.append((int(st), int(pv) + 1))

def yin(x, sr=44100, fmin=30, fmax=4500, thr=0.15):
    n = len(x); W = min(n // 2, 2048)
    if W < 64: return 0.0, 1.0
    tmax = min(int(sr / fmin), W - 1); tmin = max(2, int(sr / fmax))
    d = np.array([np.sum((x[:W] - x[t:t + W]) ** 2) for t in range(tmax + 1)])
    cm = np.ones_like(d); cs = np.cumsum(d[1:]); cm[1:] = d[1:] * np.arange(1, tmax + 1) / np.maximum(cs, 1e-9)
    for t in range(tmin, tmax):
        if cm[t] < thr and cm[t] <= cm[t + 1]:
            a, b, c = cm[t - 1], cm[t], cm[t + 1]; den = a - 2 * b + c
            return sr / (t + (0.5 * (a - c) / den if den else 0)), float(cm[t])
    t = tmin + int(np.argmin(cm[tmin:tmax])); return sr / t, float(cm[t])

def find_loop(x, period):
    L = len(x); J = 16
    att = min(L // 4, 2000)
    best = None
    for le in range(L - 48, L - J - 1):
        if le - att < 64: break
        minloop = max(int(2 * period), min(1024, (le - att) // 2), 32)
        lo, hi = att, le - minloop
        if hi <= lo + J: continue
        seg = x[lo - J:hi + J]; ref = x[le - J:le + J]
        e2 = np.convolve(seg ** 2, np.ones(2 * J), 'valid')          # sum x[ls+j]^2, ls = lo..hi
        xc = np.correlate(seg, ref, 'valid')
        err = e2 - 2 * xc + np.sum(ref ** 2)
        err /= np.sum(ref ** 2) + 1e-9
        i = int(np.argmin(err))
        if best is None or err[i] < best[0]: best = (float(err[i]), lo + i, le)
    return best

def hps(x, sr=44100):
    n = len(x)
    if n < 256: return 0.0
    X = np.abs(np.fft.rfft((x - x.mean()) * np.hanning(n), 8 * n)); fr = np.fft.rfftfreq(8 * n, 1 / sr)
    h = X.copy()
    for k in (2, 3): h[:len(X) // k] *= X[::k][:len(X) // k]
    ok = (fr > 30) & (fr < 4500); return float(fr[ok][np.argmax(h[ok])])

S = []
for n, (s, e) in enumerate(segs):
    x = rom[s:e]; L = len(x)
    fr = np.array([np.sqrt(np.mean(x[i:i + 256] ** 2)) for i in range(0, max(L - 256, 1), 256)]) if L >= 256 else np.array([np.sqrt(np.mean(x ** 2))])
    tail = fr[-1] / fr.max() if fr.max() else 0
    oneshot = tail < 0.05 or L < 200
    part = x[max(0, L - 4096):] if not oneshot else x[min(L // 8, 1000):]
    f0, ap = yin(part)
    loop = None
    if not oneshot:
        p = 44100 / f0 if f0 > 0 and ap < 0.5 else 64
        loop = find_loop(x, p)
        if loop is None or loop[0] > 0.5: oneshot, loop = True, None
    if loop: part = x[max(0, L - 4096):]      # (a repeated short loop is a comb at 44100/len: HPS would take that)
    fh = hps(part)
    if fh > 0 and ap < 0.5: f0 = fh   # harmonic product spectrum (YIN locks on harmonics of organ/guitar loops)
    if f0 > 0 and ap < 0.5: key = 69 + 12 * np.log2(f0 / 440)
    else: key = 60.0
    root = int(round(key)); corr = int(round(-(key - root) * 100))
    S.append(dict(n=n, s=s, e=e, ls=s + loop[1] if loop else 0, le=s + loop[2] if loop else 0, root=max(0, min(127, root)), corr=corr))

# envelopes (timecents / cB): atk, dcy, sus, rel
PIANO = (-12000, 2800, 960, -1200); EP = (-12000, 2400, 960, -1200); PLUCK = (-12000, 1800, 1440, -1800)
ORGAN = (-12000, -12000, 0, -2400); SUST = (-6000, -12000, 0, -1800); SLOW = (-1200, -12000, 0, 0)
PAD = (0, -12000, 0, 1200); BELL = (-12000, 2400, 1440, 0); PERC = (-12000, 1200, 1440, -1200); FX = (-2400, -12000, 0, 0)
DRUM = (-12000, -12000, 0, 1200)
# GM programs: (name, envelope, sample(s) [multisample split on roots], vibrato cents, atten cB)
FAM = [
 ("Acoustic Grand", PIANO, [0, 3, 1, 2]), ("Bright Piano", PIANO, [0, 3, 1, 2]), ("Electric Grand", PIANO, [4]),
 ("Honky-tonk", PIANO, [0, 3, 1, 2]), ("E.Piano 1", EP, [4]), ("E.Piano 2", EP, [69]), ("Harpsichord", PLUCK, [6]),
 ("Clavinet", PLUCK, [7]),
 ("Celesta", BELL, [5]), ("Glockenspiel", BELL, [5]), ("Music Box", BELL, [60]), ("Vibraphone", BELL, [69]),
 ("Marimba", PERC, [104]), ("Xylophone", PERC, [120]), ("Tubular Bells", BELL, [142]), ("Dulcimer", PLUCK, [143]),
 ("Drawbar Organ", ORGAN, [13]), ("Perc. Organ", ORGAN, [12]), ("Rock Organ", ORGAN, [9]), ("Church Organ", ORGAN, [27]),
 ("Reed Organ", ORGAN, [18]), ("Accordion", SUST, [11]), ("Harmonica", SUST, [15]), ("Tango Accordion", SUST, [10]),
 ("Nylon Guitar", PLUCK, [70]), ("Steel Guitar", PLUCK, [70]), ("Jazz Guitar", PLUCK, [71]), ("Clean Guitar", PLUCK, [72]),
 ("Muted Guitar", PERC, [72]), ("Overdrive Guitar", SUST, [39]), ("Distortion Guitar", SUST, [40]), ("Guitar Harmonics", BELL, [46]),
 ("Acoustic Bass", PLUCK, [63]), ("Finger Bass", PLUCK, [61]), ("Pick Bass", PLUCK, [80]), ("Fretless Bass", PLUCK, [38]),
 ("Slap Bass 1", PLUCK, [45]), ("Slap Bass 2", PLUCK, [62]), ("Synth Bass 1", SUST, [64]), ("Synth Bass 2", SUST, [54]),
 ("Violin", SLOW, [43]), ("Viola", SLOW, [43]), ("Cello", SLOW, [74]), ("Contrabass", SLOW, [81]),
 ("Tremolo Strings", SLOW, [75]), ("Pizzicato", PERC, [101]), ("Harp", PLUCK, [103]), ("Timpani", PERC, [92]),
 ("Strings", SLOW, [48]), ("Slow Strings", PAD, [49]), ("Synth Strings 1", SLOW, [50]), ("Synth Strings 2", PAD, [51]),
 ("Choir Aahs", SLOW, [128]), ("Voice Oohs", SLOW, [129]), ("Synth Voice", PAD, [147]), ("Orchestra Hit", PERC, [112]),
 ("Trumpet", SUST, [21]), ("Trombone", SUST, [23]), ("Tuba", SUST, [34]), ("Muted Trumpet", SUST, [30]),
 ("French Horn", SUST, [25]), ("Brass Section", SUST, [28]), ("Synth Brass 1", SUST, [29]), ("Synth Brass 2", SUST, [33]),
 ("Soprano Sax", SUST, [22]), ("Alto Sax", SUST, [37]), ("Tenor Sax", SUST, [35]), ("Baritone Sax", SUST, [41]),
 ("Oboe", SUST, [32]), ("English Horn", SUST, [36]), ("Bassoon", SUST, [65]), ("Clarinet", SUST, [24]),
 ("Piccolo", SUST, [59]), ("Flute", SUST, [44]), ("Recorder", SUST, [19]), ("Pan Flute", SUST, [26]),
 ("Blown Bottle", SUST, [88]), ("Shakuhachi", SUST, [52]), ("Whistle", SUST, [46]), ("Ocarina", SUST, [20]),
 ("Square Lead", ORGAN, [31]), ("Saw Lead", ORGAN, [8]), ("Calliope", ORGAN, [47]), ("Chiff Lead", ORGAN, [53]),
 ("Charang", ORGAN, [57]), ("Voice Lead", SUST, [58]), ("Fifths Lead", ORGAN, [55]), ("Bass + Lead", ORGAN, [56]),
 ("New Age Pad", PAD, [50]), ("Warm Pad", PAD, [51]), ("Polysynth", SUST, [128]), ("Choir Pad", PAD, [128]),
 ("Bowed Pad", PAD, [49]), ("Metallic Pad", PAD, [68]), ("Halo Pad", PAD, [147]), ("Sweep Pad", PAD, [73]),
 ("Rain", FX, [107]), ("Soundtrack", PAD, [110]), ("Crystal", BELL, [126]), ("Atmosphere", PAD, [129]),
 ("Brightness", PAD, [132]), ("Goblins", PAD, [141]), ("Echoes", PAD, [133]), ("Sci-fi", PAD, [132]),
 ("Sitar", PLUCK, [42]), ("Banjo", PLUCK, [71]), ("Shamisen", PLUCK, [72]), ("Koto", PLUCK, [109]),
 ("Kalimba", PERC, [104]), ("Bagpipe", SUST, [76]), ("Fiddle", SLOW, [43]), ("Shanai", SUST, [84]),
 ("Tinkle Bell", BELL, [5]), ("Agogo", PERC, [98]), ("Steel Drums", PERC, [60]), ("Woodblock", PERC, [101]),
 ("Taiko Drum", PERC, [92]), ("Melodic Tom", PERC, [103]), ("Synth Drum", PERC, [104]), ("Reverse Cymbal", FX, [115]),
 ("Fret Noise", PERC, [113]), ("Breath Noise", FX, [117]), ("Seashore", FX, [150]), ("Bird Tweet", FX, [99]),
 ("Telephone", FX, [106]), ("Helicopter", FX, [151]), ("Applause", FX, [118]), ("Gunshot", PERC, [93]),
]
# per program transposition in cents: register check against a free GM bank (testrig/awe/awegmcal.sh renders note 60
# of every program with this map and with TimGM6mb.sf2 and measures both; the pitch analysis above sometimes takes a
# harmonic or subharmonic of bells, plucks and synth leads)
TUNE = {}
try:
    import os
    for ln in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'awegm.tune')):
        a = ln.split()
        if len(a) == 2: TUNE[int(a[0])] = int(a[1])
except OSError: pass
VIB = {40, 41, 42, 52, 53, 56, 57, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 75, 85, 110}
# drum kit: key -> (sample, tune cents, exclusive class, pan -500..500)
DK = {35: (92, 0, 0, 0), 36: (104, -1200, 0, 0), 37: (98, 0, 0, 0), 38: (100, 0, 0, 0), 39: (94, 0, 0, -150),
      40: (102, 0, 0, 0), 41: (103, -700, 0, -300), 42: (116, 0, 1, 200), 43: (103, -400, 0, -200), 44: (113, 0, 1, 200),
      45: (103, -100, 0, -100), 46: (114, 0, 1, 200), 47: (101, -500, 0, 0), 48: (101, -300, 0, 100), 49: (115, 0, 0, -250),
      50: (101, 0, 0, 200), 51: (119, 0, 0, 250), 52: (150, 0, 0, -250), 53: (106, 0, 0, 250), 54: (108, 0, 0, 150),
      55: (144, 0, 0, -200), 56: (98, 300, 0, 150), 57: (115, 200, 0, 250), 58: (91, 0, 0, -250), 59: (119, 200, 0, 250),
      60: (101, 700, 0, 200), 61: (101, 400, 0, 200), 62: (103, 500, 0, -250), 63: (103, 800, 0, -250), 64: (103, 200, 0, -250),
      65: (101, 200, 0, 300), 66: (101, -100, 0, 300), 67: (98, 700, 0, -300), 68: (98, 400, 0, -300), 69: (108, 300, 0, -300),
      70: (105, 0, 0, -300), 71: (145, 0, 2, 300), 72: (145, -500, 2, 300), 73: (97, 300, 3, -300), 74: (97, 0, 3, -300),
      75: (98, 1200, 0, 300), 76: (101, 1200, 0, 300), 77: (101, 900, 0, 300), 78: (152, 300, 4, -300), 79: (152, 0, 4, -300),
      80: (106, 500, 5, 300), 81: (106, 500, 5, 300),
      27: (98, 900, 0, 0), 28: (94, 300, 0, 0), 29: (93, 0, 0, 0), 30: (93, -300, 0, 0), 31: (98, -300, 0, 0), 32: (97, 500, 0, 0),
      33: (98, 600, 0, 0), 34: (106, -300, 0, 0), 82: (108, 0, 0, 200), 83: (106, 900, 0, 0), 84: (106, 1200, 0, 0),
      85: (98, 0, 0, 0), 86: (92, 200, 0, 0), 87: (92, 0, 0, 0)}

used = sorted({s for f in FAM for s in f[2]} | {v[0] for v in DK.values()})
idx = {s: i for i, s in enumerate(used)}
o = []
o.append('/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */')
o.append('/* awegm.h - NUKED\'s own GM map over the AWE32 1 MB GM ROM, made by testrig/awe/awegm.py from the ROM image')
o.append('   (sample bounds, loops and pitch found by analysis; programs picked by hand). No Creative bank data. */')
o.append('#ifndef AWEGM_H\n#define AWEGM_H\n#include <stdint.h>')
o.append('typedef struct { uint32_t start, end, loopstart, loopend; int8_t root, corr; } AwegmSample;  /* loopstart 0 = one shot */')
o.append('typedef struct { uint8_t sample, keylo, keyhi; int8_t fixkey; int16_t atk, dcy, sus, rel, atten, cutoff, vib, pan, tune; uint8_t exclass; } AwegmZone;')
o.append('typedef struct { const char* name; int nz; const AwegmZone* z; } AwegmProg;')
o.append('static const AwegmSample awegm_sample[%d] = {' % len(used))
for s in used:
    d = S[s]
    o.append('    {0x%06X, 0x%06X, 0x%06X, 0x%06X, %d, %d}, /* rom #%d */' % (d['s'], d['e'], d['ls'], d['le'], d['root'], d['corr'], s))
o.append('};')
progs = []
for p, (name, env, samples) in enumerate(FAM):
    zs = []
    ss = sorted(samples, key=lambda s: S[s]['root'])
    for i, s in enumerate(ss):
        lo = 0 if i == 0 else (S[ss[i - 1]]['root'] + S[s]['root']) // 2 + 1
        hi = 127 if i == len(ss) - 1 else (S[s]['root'] + S[ss[i + 1]]['root']) // 2
        vib = 15 if p in VIB else 0
        zs.append('{%d, %d, %d, -1, %d, %d, %d, %d, 0, 0, %d, 0, %d, 0}' % (idx[s], lo, hi, env[0], env[1], env[2], env[3], vib, TUNE.get(p, 0)))
    o.append('static const AwegmZone awegm_z%d[] = { %s };' % (p, ', '.join(zs)))
    progs.append('{"%s", %d, awegm_z%d}' % (name, len(zs), p))
dz = []
for k in sorted(DK):
    s, tune, ex, pan = DK[k]
    dz.append('{%d, %d, %d, %d, %d, %d, %d, %d, 0, 0, 0, %d, %d, %d}' % (idx[s], k, k, -1, DRUM[0], DRUM[1], DRUM[2], DRUM[3], pan, tune, ex))
o.append('static const AwegmZone awegm_z128[] = {\n    ' + ',\n    '.join(dz) + '\n};')
progs.append('{"Standard Kit", %d, awegm_z128}' % len(dz))
o.append('static const AwegmProg awegm_prog[129] = {\n    ' + ',\n    '.join(progs) + '\n};')
o.append('#endif')
print('\n'.join(o))
for s in used:
    d = S[s]
    print('rom #%d len %d loop %s root %d' % (s, d['e'] - d['s'], (d['ls'] - d['s'], d['le'] - d['s']) if d['ls'] else '-', d['root']), file=sys.stderr)
