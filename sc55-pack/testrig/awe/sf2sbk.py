#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""sf2sbk.py IN.SF2 OUT.SBK [gm|gs|mt32] [MAXPRESETS] - a stand-in for Creative's SYNTHGM/GS/MT.SBK made from a free SF2
(SoundFont 1 file, samples in sample RAM; the real ones play from the GM ROM). Every preset zone x instrument zone is
flattened to SF2 values (additive rules), then written in SBK units (the inverse of awesfx's sbkconv.c). Sample rates
are folded into the SBK sample pitch (SBK has none). gs: adds variation banks 8/16 (copies), mt32: MT-32 tone names on
programs 7/8/16/24 (what awemidi.c's content check looks for), gm: as is. The INAM is neutral ("Test bank") so the
kind is found from the presets, not the name."""
import sys, struct, math
src, dst = sys.argv[1], sys.argv[2]
kind = sys.argv[3] if len(sys.argv) > 3 else 'gm'
d = open(src, 'rb').read()
ch = {}
def walk(p, end):
    while p + 8 <= end:
        cid = d[p:p + 4]; sz = struct.unpack('<I', d[p + 4:p + 8])[0]
        if cid == b'LIST': walk(p + 12, p + 8 + sz)
        else: ch[cid] = d[p + 8:p + 8 + sz]
        p += 8 + sz + (sz & 1)
walk(12, len(d))
def recs(cid, n, fmt): b = ch[cid]; return [struct.unpack(fmt, b[i:i + n]) for i in range(0, len(b) - n + 1, n)]
phdr = recs(b'phdr', 38, '<20sHHHIII'); pbag = recs(b'pbag', 4, '<HH'); pgen = recs(b'pgen', 4, '<Hh')
inst = recs(b'inst', 22, '<20sH'); ibag = recs(b'ibag', 4, '<HH'); igen = recs(b'igen', 4, '<Hh')
shdr = recs(b'shdr', 46, '<20sIIIIIBbHH'); smpl = ch[b'smpl']
DEF = {8: 13500, 21: -12000, 23: -12000, 25: -12000, 26: -12000, 27: -12000, 28: -12000, 30: -12000, 33: -12000, 34: -12000,
       35: -12000, 36: -12000, 38: -12000, 43: 0x7F00, 44: 0x7F00, 46: -1, 47: -1, 56: 100, 58: -1}
RANGE = {43, 44}; OVWRT = {46, 47, 54, 57, 58}
def layers(bags, gens, b0, b1):
    out = []
    for b in range(b0, b1):
        g0, g1 = bags[b][0], bags[b + 1][0]
        out.append({o: a for o, a in gens[g0:g1]})
    return out
def merge(dst_, src_):   # preset values onto instrument values (SF2 rules)
    r = dict(dst_)
    for o, a in src_.items():
        if o in (41, 53): continue
        if o in RANGE:
            v = r.get(o, 0x7F00)
            lo = max(v & 0xFF, a & 0xFF); hi = min((v >> 8) & 0xFF, (a >> 8) & 0xFF); r[o] = lo | hi << 8
        elif o in OVWRT: r[o] = a
        else: r[o] = r.get(o, DEF.get(o, 0)) + a
    return r
def u16(v): return v & 0xFFFF
def sbk(o, v):   # SF2 value -> SBK value (sbkconv.c inverse)
    clamp = lambda x, a, b: max(a, min(b, int(round(x))))
    if o == 8: return 127 if v >= 14400 else clamp((v - 4366) / 59, 0, 126)
    if o == 9: return clamp(v * 2 / 3, 0, 127)
    if o in (15, 16): return clamp(v * 256 / 1000, 0, 255)
    if o == 17: return clamp((v + 500) * 127 / 1000, 0, 127)
    if o == 48: return 0 if v >= 1000 else clamp(127 * 10 ** (-v / 2000), 1, 127)
    if o == 56: return 1 if v <= 75 else 0
    if o in (21, 23, 25, 26, 27, 28, 30, 33, 34, 35, 36, 38): return clamp(1000 * 2 ** (v / 1200), 1, 32767)
    if o in (31, 32, 39, 40): return clamp(v / 5.55, -127, 127)
    if o in (22, 24): return clamp(2 ** ((v + 7925) / 1200), 1, 255)
    if o in (5, 6, 7): return clamp(v * 64 / 600, -127, 127)
    if o == 10: return clamp(v * 64 / 3600, -127, 127)
    if o == 11: return clamp(v * 64 / 7200, -127, 127)
    if o == 13: return clamp(v * 64 / 120, -127, 127)
    if o == 29: return clamp(96 - v * 96 / 1000, 0, 96)
    if o == 37: return clamp((2000 - 2 * v) / 21, 0, 96)
    return v
MT = {7: 'Honkytonk', 8: 'Elec Org 1', 16: 'Harpsi 1', 24: 'Celesta 1'}
maxp = int(sys.argv[4]) if len(sys.argv) > 4 else 9999
# samples used, laid out from RAM address 0 with 48 zero words after each
presets = []
for i in range(len(phdr) - 1):
    name, prog, bank = phdr[i][0].split(b'\0')[0], phdr[i][1], phdr[i][2]
    if bank not in (0, 128): continue
    presets.append((name, prog, bank, i))
presets = presets[:maxp]
if kind == 'gs':
    extra = [(n + b' var', p, b, i) for (n, p, b, i) in presets if b == 0 and p < 8]
    presets += [(n, p, 8, i) for (n, p, b, i) in extra] + [(n, p, 16, i) for (n, p, b, i) in extra]
out_inst, out_presets, used = [], [], {}
for (name, prog, bank, pi) in presets:
    if kind == 'mt32' and bank == 0 and prog in MT: name = MT[prog].encode()
    pz = []
    pl = layers(pbag, pgen, phdr[pi][3], phdr[pi + 1][3])
    pglob = pl[0] if pl and 41 not in pl[0] else {}
    for z in pl:
        if 41 not in z: continue
        pzv = dict(pglob); pzv.update(z)
        ii = z[41]
        il = layers(ibag, igen, inst[ii][1], inst[ii + 1][1])
        iglob = il[0] if il and 53 not in il[0] else {}
        zones = []
        for iz in il:
            if 53 not in iz: continue
            v = dict(iglob); v.update(iz)
            v = merge(v, pzv)
            s = v[53]
            if s not in used: used[s] = len(used)
            zones.append(v)
        if zones:
            out_inst.append((name[:19], zones))
            pz.append(len(out_inst) - 1)
    out_presets.append((name, prog, bank, pz))
# sample data
order = sorted(used, key=lambda s: used[s]); pos = 0; data = bytearray(); sinfo = {}
for s in order:
    nm, start, end, sl, el, rate, pitch, corr, link, typ = shdr[s]
    sinfo[s] = (pos, pos + end - start, pos + sl - start, pos + el - start, rate, pitch, corr)
    data += smpl[start * 2:end * 2] + bytes(96); pos += end - start + 48
gens_p, bags_p, gens_i, bags_i, insts, phdrs = [], [], [], [], [], []
for k, (nm, zones) in enumerate(out_inst):
    insts.append(struct.pack('<20sH', nm.ljust(20, b'\0'), len(bags_i)))
    for v in zones:
        bags_i.append(struct.pack('<HH', len(gens_i), 0))
        s = v[53]; p0, p1, l0, l1, rate, pitch, corr = sinfo[s]
        root = v.get(58, -1); root = pitch if root < 0 else root
        cents = root * 100 - corr + 1200 * math.log2(44100 / rate)    # rate folded into the pitch
        g = []
        if 43 in v: g.append((43, v[43]))
        if 44 in v: g.append((44, v[44]))
        for o in sorted(v):
            if o in (43, 44, 53, 58, 41): continue
            g.append((o, sbk(o, v[o])))
        for o, dv in ((29, 0), (37, 0), (22, 0), (24, 0)):   # SBK defaults differ: write the SF2 default
            if o not in v: g.append((o, sbk(o, dv)))
        g.append((55, int(round(cents))))
        g.append((53, used[s]))
        gens_i += [struct.pack('<Hh', o, max(-32768, min(32767, a)) if o not in RANGE else 0) if o not in RANGE else struct.pack('<HH', o, a) for o, a in g]
insts.append(struct.pack('<20sH', b'EOI'.ljust(20, b'\0'), len(bags_i)))
bags_i.append(struct.pack('<HH', len(gens_i), 0)); gens_i.append(struct.pack('<Hh', 0, 0))
for (nm, prog, bank, pz) in out_presets:
    phdrs.append(struct.pack('<20sHHHIII', nm[:20].ljust(20, b'\0'), prog, bank, len(bags_p), 0, 0, 0))
    for k in pz:
        bags_p.append(struct.pack('<HH', len(gens_p), 0))
        gens_p.append(struct.pack('<Hh', 41, k))
phdrs.append(struct.pack('<20sHHHIII', b'EOP'.ljust(20, b'\0'), 255, 255, len(bags_p), 0, 0, 0))
bags_p.append(struct.pack('<HH', len(gens_p), 0)); gens_p.append(struct.pack('<Hh', 0, 0))
shdrs = []; snam = b''
for s in order:
    p0, p1, l0, l1 = sinfo[s][:4]
    shdrs.append(struct.pack('<IIII', p0, p1, l0 - 1, l1 - 2))      # awesfx adds 1 / 2 back
    snam += shdr[s][0][:20].ljust(20, b'\0')
def chunk(cid, b): return cid + struct.pack('<I', len(b)) + b + (b'\0' if len(b) & 1 else b'')
def lst(cid, b): return chunk(b'LIST', cid + b)
info = lst(b'INFO', chunk(b'ifil', struct.pack('<HH', 1, 0)) + chunk(b'INAM', b'Test bank\0') + chunk(b'irom', b'1MGM\0'))
sdta = lst(b'sdta', chunk(b'snam', snam) + chunk(b'smpl', bytes(data)))
pdta = lst(b'pdta', chunk(b'phdr', b''.join(phdrs)) + chunk(b'pbag', b''.join(bags_p)) + chunk(b'pmod', b'\0' * 10) +
           chunk(b'pgen', b''.join(gens_p)) + chunk(b'inst', b''.join(insts)) + chunk(b'ibag', b''.join(bags_i)) +
           chunk(b'imod', b'\0' * 10) + chunk(b'igen', b''.join(gens_i)) + chunk(b'shdr', b''.join(shdrs)))
body = b'sfbk' + info + sdta + pdta
open(dst, 'wb').write(b'RIFF' + struct.pack('<I', len(body)) + body)
print('%s: %d presets, %d instruments, %d samples, %d KB sample RAM' % (dst, len(out_presets), len(out_inst), len(order), len(data) // 1024))
