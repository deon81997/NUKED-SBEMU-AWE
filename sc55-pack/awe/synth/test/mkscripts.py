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

s = S("t12_sends", "reverb/chorus sends set (both engines start as after AWEUTIL /S: Hall 2, Chorus 3)")
a = tone(s, RAM, "sine", 120)
s.note(0, *a, rev=0x80, cho=0x40); s.ms(500); s.off(0); s.ms(200)
scripts.append(s)

# Reverb / chorus types as Creative's drivers program them (values: Linux sound/isa/sb/emu8000.c, from the AWE SDK)
REV = {
 "room1": (0xB488,0xA450,0x9550,0x84B5,0x383A,0x3EB5,0x72F4,0x72A4,0x7254,0x7204,0x7204,0x7204,0x4416,0x4516,0xA490,0xA590,0x842A,0x852A,0x842A,0x852A,0x8429,0x8529,0x8429,0x8529,0x8428,0x8528,0x8428,0x8528),
 "room2": (0xB488,0xA458,0x9558,0x84B5,0x383A,0x3EB5,0x7284,0x7254,0x7224,0x7224,0x7254,0x7284,0x4448,0x4548,0xA440,0xA540,0x842A,0x852A,0x842A,0x852A,0x8429,0x8529,0x8429,0x8529,0x8428,0x8528,0x8428,0x8528),
 "room3": (0xB488,0xA460,0x9560,0x84B5,0x383A,0x3EB5,0x7284,0x7254,0x7224,0x7224,0x7254,0x7284,0x4416,0x4516,0xA490,0xA590,0x842C,0x852C,0x842C,0x852C,0x842B,0x852B,0x842B,0x852B,0x842A,0x852A,0x842A,0x852A),
 "hall1": (0xB488,0xA470,0x9570,0x84B5,0x383A,0x3EB5,0x7284,0x7254,0x7224,0x7224,0x7254,0x7284,0x4448,0x4548,0xA440,0xA540,0x842B,0x852B,0x842B,0x852B,0x842A,0x852A,0x842A,0x852A,0x8429,0x8529,0x8429,0x8529),
 "hall2": (0xB488,0xA470,0x9570,0x84B5,0x383A,0x3EB5,0x7254,0x7234,0x7224,0x7254,0x7264,0x7294,0x44C3,0x45C3,0xA404,0xA504,0x842A,0x852A,0x842A,0x852A,0x8429,0x8529,0x8429,0x8529,0x8428,0x8528,0x8428,0x8528),
 "plate": (0xB4FF,0xA470,0x9570,0x84B5,0x383A,0x3EB5,0x7234,0x7234,0x7234,0x7234,0x7234,0x7234,0x4448,0x4548,0xA440,0xA540,0x842A,0x852A,0x842A,0x852A,0x8429,0x8529,0x8429,0x8529,0x8428,0x8528,0x8428,0x8528),
 "delay": (0xB4FF,0xA470,0x9500,0x84B5,0x333A,0x39B5,0x7204,0x7204,0x7204,0x7204,0x7204,0x72F4,0x4400,0x4500,0xA4FF,0xA5FF,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520),
 "pandelay": (0xB4FF,0xA490,0x9590,0x8474,0x333A,0x39B5,0x7204,0x7204,0x7204,0x7204,0x7204,0x72F4,0x4400,0x4500,0xA4FF,0xA5FF,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520,0x8420,0x8520),
}
REV_CMDS = [("INIT1",3),("INIT1",5),("INIT4",0x1F),("INIT1",7),("INIT2",0x14),("INIT2",0x16),("INIT1",0xF),("INIT1",0x17),
            ("INIT1",0x1F),("INIT2",7),("INIT2",0xF),("INIT2",0x17),("INIT2",0x1D),("INIT2",0x1F),("INIT3",1),("INIT3",3),
            ("INIT1",9),("INIT1",0xB),("INIT1",0x11),("INIT1",0x13),("INIT1",0x19),("INIT1",0x1B),("INIT2",1),("INIT2",3),
            ("INIT2",9),("INIT2",0xB),("INIT2",0x11),("INIT2",0x13)]
CHO = {  # feedback, delay offset, lfo depth, right delay, lfo freq
 "chorus1": (0xE600,0x03F6,0xBC2C,0x00000000,0x0000006D), "chorus2": (0xE608,0x031A,0xBC6E,0x00000000,0x0000017C),
 "chorus3": (0xE610,0x031A,0xBC84,0x00000000,0x00000083), "chorus4": (0xE620,0x0269,0xBC6E,0x00000000,0x0000017C),
 "feedback": (0xE680,0x04D3,0xBCA6,0x00000000,0x0000005B), "flanger": (0xE6E0,0x044E,0xBC37,0x00000000,0x00000026),
 "shortdelay": (0xE600,0x0B06,0xBC00,0x0006E000,0x00000083), "shortdelayfb": (0xE6C0,0x0B06,0xBC00,0x0006E000,0x00000083),
}

def set_rev(s, name):
    for (r, ch), v in zip(REV_CMDS, REV[name]):
        s.w(r, ch, v)

def set_cho(s, name):
    fb, off, depth, delay, freq = CHO[name]
    s.w("INIT3", 9, fb); s.w("INIT3", 12, off); s.w("INIT4", 3, depth)
    s.w("HWCF4", 0, delay); s.w("HWCF5", 0, freq); s.w("HWCF6", 0, 0x8000); s.w("HWCF7", 0, 0)

for i, name in enumerate(REV):
    s = S("t12_rev%d_%s" % (i, name), "reverb %s (send FF, dry note 250 ms, 2 s tail)" % name)
    set_rev(s, name); set_cho(s, "chorus3")
    a = tone(s, RAM, "saw", 150)
    s.note(0, *a, rev=0xFF, att=0x20); s.ms(250); s.off(0, 0x7F); s.ms(2000)
    scripts.append(s)
for i, name in enumerate(CHO):
    s = S("t13_cho%d_%s" % (i, name), "chorus %s (send FF, 3 s sustained note)" % name)
    set_rev(s, "hall2"); set_cho(s, name)
    a = tone(s, RAM, "saw", 150)
    s.note(0, *a, cho=0xFF, att=0x20); s.ms(3000); s.off(0, 0x7F); s.ms(500)
    scripts.append(s)

# t14: what the AWE32 SDK drivers (Apogee in Duke 3D, HMI SOS in Descent) see: no init of their own (they rely on
# AWEUTIL /S), free voices found by reading DCYSUSV (bit 7 set = not for notes) and VTFT right after a note-on.
# The note is Descent's first one, register for register from its port stream (testrig/awe/cap.sh). Checked in
# run_tests.sh (ours only): idle voices 807F, refresh voices 0080, VTFT volume near 0 at the attack's start.
s = S("t14_sdk", "SDK driver read-backs: idle DCYSUSV, VTFT right after a note-on", rom=True)
s.l = [x for x in s.l if not x.startswith("w ")]         # no set-up: the game's driver writes none
s.l += ["rd DCYSUSV 0", "rd DCYSUSV 29", "rd DCYSUSV 30", "rd DCYSUSV 31"]
for reg, v in (("DCYSUSV", 0x807F), ("IP", 0xE000), ("DCYSUSV", 0x0080), ("VTFT", 0x0000FFFF), ("ENVVOL", 0x8000),
               ("ATKHLDV", 0x7F7D), ("ENVVAL", 0x8000), ("ATKHLD", 0x7F7D), ("DCYSUS", 0x004D), ("IP", 0xCAB8),
               ("IFATN", 0x1425), ("LFO1VAL", 0x8000), ("LFO2VAL", 0x8000), ("PEFE", 0x0072), ("FMMOD", 0),
               ("TREMFRQ", 0x0080), ("FM2FRQ2", 0), ("PTRX", 0x19744000), ("PSST", 0x9903BF7E), ("CSL", 0x0003C129),
               ("CCCA", 0x0003BF48), ("DCYSUSV", 0x0201)):
    s.w(reg, 29, v)
s.l += ["rd VTFT 29", "rd DCYSUSV 29", "ms 100", "rd VTFT 29"]
scripts.append(s)

for s in scripts:
    s.save()
print("wrote %d scripts to %s" % (len(scripts), OUT))
