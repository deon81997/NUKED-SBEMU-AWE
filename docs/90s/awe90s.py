#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""AWE32 edition graphics for README.md (all original, made with Pillow, in the style of make90s.py / voxels.py):
a voxel full-length wavetable card, an 88x31 "AWE32 synth INSIDE" button and the "AWE32 EDITION" banner.

    python3 docs/90s/awe90s.py      -> writes vox-awe.gif, vox-part-awe.gif, btn-awe32.gif, awe-edition.gif
"""
import math
from PIL import Image, ImageDraw
from make90s import button, save, crisp, rainbow, F
from voxels import box, render


def awecard():
    m = {}
    box(m, 0, 0, 0, 30, 1, 11, (20, 105, 55))                       # full-length PCB
    box(m, -2, -1, 0, 0, 2, 13, (170, 170, 180))                    # bracket
    for z, c in ((2, (230, 150, 40)), (4, (60, 160, 230)), (6, (230, 90, 160)), (8, (90, 220, 90))): m[(-3, 0, z)] = c   # jacks
    for x in range(2, 27, 2): box(m, x, 0, -2, x + 1, 1, 0, (230, 180, 40))   # ISA gold fingers
    box(m, 3, -2, 3, 8, 0, 8, (25, 25, 30))                         # DSP
    box(m, 4, -3, 5, 7, -2, 6, (230, 230, 240))                     #   its label dot
    box(m, 10, -3, 2, 17, 0, 9, (30, 30, 40))                       # EMU8000, the big one
    box(m, 11, -4, 5, 16, -3, 6, (255, 140, 0))                     #   orange stripe (the wavetable!)
    box(m, 19, -2, 3, 22, 0, 6, (25, 25, 30))                       # OPL3
    for x in (23, 26):                                              # two SIMM sockets, one with sample RAM
        box(m, x, -1, 1, x + 2, 0, 10, (235, 235, 225))
    box(m, 23, -3, 2, 24, -1, 10, (40, 120, 40))                    # the SIMM stick
    for z in range(3, 10, 2): box(m, 23, -4, z, 24, -3, z + 1, (25, 25, 30))
    box(m, 3, -1, 9, 17, 0, 10, (200, 200, 210))                    # game/MIDI header
    box(m, 28, -1, 6, 29, 0, 9, (220, 210, 90))                     # crystal
    return m


def banner():
    W, H, nf = 300, 40, 12
    big, small = F('DejaVuSans-Bold.ttf', 22), F('DejaVuSansCondensed-Bold.ttf', 9)
    text = '~*~ AWE32 EDITION ~*~'
    frames = []
    for k in range(nf):
        im = Image.new('RGB', (W, H), (0, 0, 32))
        d = crisp(ImageDraw.Draw(im))
        tw = d.textlength(text, font=big); x = (W - tw) / 2
        for i, ch in enumerate(text):
            y = 3 + 3 * math.sin(2 * math.pi * (k / nf + i / 10))
            d.text((x + 1, y + 1), ch, font=big, fill=(0, 0, 0))
            d.text((x, y), ch, font=big, fill=rainbow((k / nf + i / len(text)) % 1))
            x += d.textlength(ch, font=big)
        sub = 'Sound Blaster AWE32 / AWE64 wavetable in plain DOS!!'
        d.text(((W - d.textlength(sub, font=small)) / 2, 29), sub, font=small, fill=(255, 255, 0) if k % 4 < 2 else (255, 140, 0))
        for s in range(6):                                          # twinkles
            sx, sy = (37 * s + 11 * k) % W, (5 + 13 * s) % 26
            if (s + k) % 3 == 0: d.point([(sx, sy), (sx - 1, sy), (sx + 1, sy), (sx, sy - 1), (sx, sy + 1)], fill=(255, 255, 255))
        frames.append(im)
    save(frames, 'awe-edition.gif', 110)


if __name__ == '__main__':
    render(awecard(), (240, 150), scale=6, tilt=0.6, name='vox-awe.gif')
    render(awecard(), (180, 110), scale=4.2, tilt=0.6, name='vox-part-awe.gif', cap='AWE32 / AWE64|EMU8000 wavetable')
    button('btn-awe32.gif', 'AWE32 synth', 'INSIDE', (0, 0, 64), (255, 140, 0), ((255, 140, 0), '32', (0, 0, 0)), anim=True)
    banner()
