#!/usr/bin/env python3
"""Write the hand-made EMU8000 register scripts into test/scripts/.

Note on/off follow the order AWE drivers use (Linux awe_wave.c style):
stop the envelope engine, set targets/currents, envelope + LFO + pitch +
attenuation regs, loop/pan/sends, address, then DCYSUSV with the engine bit
clear (= trigger). Run: python3 mkscripts.py [outdir]
"""
import os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "scripts")
RAM = 0x200000


class S:
    def __init__(self, name, desc, rom=False):
        self.name, self.l = name, ["# " + desc, "ram 512"]
        if rom:
            self.l.append("rom $AWEROM")
        self.init()

    def w(self, reg, ch, val):
        self.l.append("w %s %d 0x%X" % (reg, ch, val & 0xFFFFFFFF))

    def ms(self, n):
        self.l.append("ms %d" % n)

    def init(self):
        self.w("HWCF1", 0, 0x59)
        self.w("HWCF2", 0, 0x20)
        for ch in range(32):
            self.w("DCYSUSV", ch, 0x80)
            for r in ("ENVVOL", "ENVVAL", "DCYSUS", "ATKHLDV", "LFO1VAL", "ATKHLD", "LFO2VAL",
                      "IP", "IFATN", "PEFE", "FMMOD", "TREMFRQ", "FM2FRQ2"):
                self.w(r, ch, 0)
            for r in ("PTRX", "VTFT", "CVCF", "PSST", "CSL", "CCCA", "CPF"):
                self.w(r, ch, 0)
        self.w("HWCF3", 0, 4)

    def fill(self, addr, length, kind, period, amp=16000):
        self.l.append("fill 0x%X %d %s %g %d" % (addr, length, kind, period, amp))

    def note(self, ch, start, ls, le, ip=0xE000, att=0, cut=0xFF, q=0, pan=0x80,
             atkhldv=0x7F7F, dcysusv=0x7F7F, envvol=0x8000, envval=0x8000, atkhld=0x7F7F,
             dcysus=0x7F7F, pefe=0, fmmod=0, tremfrq=0, fm2frq2=0, lfo1val=0x8000,
             lfo2val=0x8000, rev=0, cho=0):
        w = self.w
        w("DCYSUSV", ch, 0x80)
        w("VTFT", ch, 0x0000FFFF)
        w("CVCF", ch, 0x0000FFFF)
        w("PTRX", ch, 0x40000000 | (rev << 8))
        w("CPF", ch, 0x40000000)
        w("ENVVOL", ch, envvol)
        w("ENVVAL", ch, envval)
        w("DCYSUS", ch, dcysus)
        w("ATKHLDV", ch, atkhldv)
        w("LFO1VAL", ch, lfo1val)
        w("ATKHLD", ch, atkhld)
        w("LFO2VAL", ch, lfo2val)
        w("IP", ch, ip)
        w("IFATN", ch, (cut << 8) | att)
        w("PEFE", ch, pefe)
        w("FMMOD", ch, fmmod)
        w("TREMFRQ", ch, tremfrq)
        w("FM2FRQ2", ch, fm2frq2)
        w("PSST", ch, (pan << 24) | ls)
        w("CSL", ch, (cho << 24) | le)
        w("CCCA", ch, (q << 28) | start)
        w("DCYSUSV", ch, dcysusv)

    def off(self, ch, rel=0x7F, mrel=0x7F):
        self.w("DCYSUSV", ch, 0x8000 | rel)
        self.w("DCYSUS", ch, 0x8000 | mrel)

    def save(self):
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, self.name + ".awe"), "w") as f:
            f.write("\n".join(self.l) + "\n")


def tone(s, addr, kind, period, n=64):
    """sample at addr: n periods, loop over the last half; returns (start, ls, le)"""
    s.fill(addr, int(period * n) + 8, kind, period)
    return addr, addr + int(period * n / 2), addr + int(period * n)


scripts = []

s = S("t01_sine", "441 Hz looped sine, fast attack, release")
a = tone(s, RAM, "sine", 100)
s.note(0, *a)
s.ms(600); s.off(0, 0x60); s.ms(400)
scripts.append(s)

s = S("t02_pitch", "octave/semitone steps of IP on a saw")
a = tone(s, RAM, "saw", 200)
for ip in (0xE000, 0xD000, 0xF000, 0xE155, 0xE2AB, 0xC800):
    s.note(0, *a, ip=ip); s.ms(250); s.off(0); s.ms(60)
scripts.append(s)

s = S("t03_env", "volume envelope: delay, slow attack, hold, decay to sustain, release")
a = tone(s, RAM, "sine", 100)
s.note(0, *a, envvol=0x7E00, atkhldv=0x7A30, dcysusv=0x4050)
s.ms(1500); s.off(0, 0x58); s.ms(1200)
scripts.append(s)

s = S("t04_filter", "saw: filter env sweep, Q 0 then Q 15, then static low cutoff")
a = tone(s, RAM, "saw", 300)
for q in (0, 15):
    s.note(0, *a, cut=0x20, q=q, pefe=0x007F, atkhld=0x7F38, dcysus=0x2050)
    s.ms(1200); s.off(0); s.ms(100)
s.note(0, *a, cut=0x60, q=8); s.ms(600); s.off(0); s.ms(100)
scripts.append(s)

s = S("t05_lfo", "LFO1 vibrato + tremolo + filter, LFO2 vibrato with delay")
a = tone(s, RAM, "saw", 200)
s.note(0, *a, fmmod=0x2000, tremfrq=0x4020, cut=0x90, q=4); s.ms(1200); s.off(0); s.ms(100)
s.note(1, *a, fmmod=0x0040, cut=0x80, q=10, tremfrq=0x0030); s.ms(1000); s.off(1); s.ms(100)
s.note(2, *a, fm2frq2=0x3018, lfo2val=0x6000); s.ms(1500); s.off(2); s.ms(100)
scripts.append(s)

s = S("t06_pan", "pan positions 0x00..0xFF on 5 voices in turn")
a = tone(s, RAM, "sine", 80)
for i, p in enumerate((0x00, 0x40, 0x80, 0xC0, 0xFF)):
    s.note(i, *a, pan=p); s.ms(300); s.off(i); s.ms(50)
scripts.append(s)

s = S("t07_32voices", "32 voices at once, spread pitches and pans, filters on")
a = tone(s, RAM, "saw", 400)
for ch in range(32):
    s.note(ch, *a, ip=0xC000 + ch * 0x155, att=0x30, pan=(ch * 8) & 0xFF, cut=0xA0, q=ch % 16,
           fmmod=0x0810, tremfrq=0x1018 + ch)
s.ms(1500)
for ch in range(32):
    s.off(ch, 0x50)
s.ms(600)
scripts.append(s)

s = S("t08_rom", "ROM sample playback (needs AWEROM), loop inside ROM", rom=True)
for i, ip in enumerate((0xE000, 0xD800, 0xE400)):
    s.note(i, 0x10000 + i * 0x4000, 0x11000 + i * 0x4000, 0x12000 + i * 0x4000, ip=ip, att=0x10)
    s.ms(400); s.off(i); s.ms(80)
scripts.append(s)

s = S("t09_direct", "envelope engine off: software drives VTFT/CVCF like trackers")
a = tone(s, RAM, "square", 150)
s.w("DCYSUSV", 0, 0x80); s.w("PSST", 0, (0x80 << 24) | a[1]); s.w("CSL", 0, a[2])
s.w("CCCA", 0, a[0]); s.w("PTRX", 0, 0x40000000); s.w("CPF", 0, 0x40000000)
for v in (0x8000, 0x4000, 0xFFFF, 0x2000, 0):
    s.w("VTFT", 0, (v << 16) | 0xFFFF); s.ms(200)
scripts.append(s)

s = S("t10_modpitch", "mod envelope to pitch (+/-) and slow decay")
a = tone(s, RAM, "saw", 200)
s.note(0, *a, pefe=0x7F00, atkhld=0x7F20, dcysus=0x0040); s.ms(1200); s.off(0); s.ms(100)
s.note(0, *a, pefe=0x8000, atkhld=0x6060, dcysus=0x4030); s.ms(1200); s.off(0); s.ms(100)
scripts.append(s)

s = S("t11_reads", "register read-back (compare the printed values)")
a = tone(s, RAM, "sine", 100)
s.note(3, *a, ip=0xE800, att=0x20, cut=0x80, q=5)
s.ms(100)
for r in ("PSST", "CSL", "IP", "IFATN", "ATKHLDV", "DCYSUSV", "HWCF1", "HWCF2", "HWCF3", "SMALW", "ID"):
    s.l.append("rd %s 3" % r)
s.l.append("ior 0x802")
s.ms(10)
scripts.append(s)

s = S("t12_sends", "reverb/chorus sends set (effects are stubs: dry must match)")
a = tone(s, RAM, "sine", 120)
s.note(0, *a, rev=0x80, cho=0x40); s.ms(500); s.off(0); s.ms(200)
scripts.append(s)

for s in scripts:
    s.save()
print("wrote %d scripts to %s" % (len(scripts), OUT))
