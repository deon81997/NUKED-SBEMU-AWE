#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
"""emustream.py - EMU8000 port streams from cap.sh (86Box EMU8K_TRACE): decode, diff, export.

  emustream.py dump TRACE [N]          decoded word ops (register names), first N
  emustream.py diff A B [N]            align two streams by their writes; list reads that differ (first N)
  emustream.py awe TRACE OUT.awe [HZ]  harness script (io/ior/wait) for test/harness (emu8k.c and 86Box ref)
Byte records (SBEMU's trap sees bytes) are joined into words: even byte then odd byte of the same word port.
Times: 86Box TSC (HZ, default 400 MHz = P2B-LS PII-400) -> 44.1 kHz frames."""
import sys

R0 = ['CPF', 'PTRX', 'CVCF', 'VTFT', 'Z2', 'Z1', 'PSST', 'CSL']
R3 = ['IP', 'IFATN', 'PEFE', 'FMMOD', 'TREMFRQ', 'FM2FRQ2', 'R3_6', 'ID']
R12 = {(1, 2): 'INIT1', (1, 3): 'INIT3', (1, 4): 'ENVVOL', (1, 5): 'DCYSUSV', (1, 6): 'ENVVAL', (1, 7): 'DCYSUS',
       (2, 2): 'INIT2', (2, 3): 'INIT4', (2, 4): 'ATKHLDV', (2, 5): 'LFO1VAL', (2, 6): 'ATKHLD', (2, 7): 'LFO2VAL'}
CFG = {9: 'HWCF4', 10: 'HWCF5', 13: 'HWCF6', 14: 'HWCF7', 20: 'SMALR', 21: 'SMARR', 22: 'SMALW', 23: 'SMARW',
       26: 'SMxD', 27: 'WC', 29: 'HWCF1', 30: 'HWCF2', 31: 'HWCF3'}


def load(path):
    ops, pend = [], {}
    for ln in open(path):
        p = ln.split()
        if len(p) != 4:
            continue
        t, k, off, v = int(p[0]), p[1], int(p[2], 16), int(p[3], 16)
        if k in 'WR':
            ops.append((t, k, off, v))
            continue
        w = off & ~1
        if not off & 1:
            pend[(k, w)] = (t, v & 0xFF)
            if k == 'w' and w == 0x802:
                ops.append((t, 'W', 0x802, v & 0xFF))
        else:
            t0, lo = pend.pop((k, w), (t, None))
            if lo is None:
                ops.append((t, k.upper() + 'h', w, v))   # lone high byte
            elif not (k == 'w' and w == 0x802):
                ops.append((t0, k.upper(), w, lo | (v & 0xFF) << 8))
    return ops


def name(ptr, off):
    reg, ch = ptr >> 5 & 7, ptr & 31
    port = {0x000: 0, 0x002: 0, 0x400: 1, 0x402: 2, 0x800: 3}.get(off)
    if port is None:
        return 'PTR'
    if port == 0:
        n = R0[reg] + ('.hi' if off == 2 else '.lo')
    elif port == 3:
        n = R3[reg]
    elif reg == 0:
        n = 'CCCA' + ('.hi' if port == 2 else '.lo')
    elif reg == 1:
        n = CFG.get(ch, 'CFG?%d' % ch) + ('.p%d' % port)
        return n
    else:
        n = R12[(port, reg)]
    return '%s[%d]' % (n, ch)


def decoded(ops):
    ptr = 0
    for t, k, off, v in ops:
        if off == 0x802 and k == 'W':
            ptr = v
            continue
        yield t, k, name(ptr, off), v


def main():
    cmd = sys.argv[1]
    if cmd == 'dump':
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 200
        for i, (t, k, nm, v) in enumerate(decoded(load(sys.argv[2]))):
            if i >= n:
                break
            print('%12d %s %-16s %04X' % (t, k, nm, v))
    elif cmd == 'diff':
        a, b = list(decoded(load(sys.argv[2]))), list(decoded(load(sys.argv[3])))
        n = int(sys.argv[4]) if len(sys.argv) > 4 else 40
        i = j = shown = 0
        reads = {}
        while i < len(a) and j < len(b) and shown < n:
            ta, ka, na, va = a[i]
            tb, kb, nb, vb = b[j]
            if (ka, na) == (kb, nb):
                if ka == 'R' and va != vb:
                    key = na.split('[')[0]
                    reads.setdefault(key, 0)
                    reads[key] += 1
                    if reads[key] <= 3:
                        print('read  #%d/%d %-16s A=%04X B=%04X' % (i, j, na, va, vb)); shown += 1
                elif ka == 'W' and va != vb:
                    print('write #%d/%d %-16s A=%04X B=%04X' % (i, j, na, va, vb)); shown += 1
                i += 1; j += 1
                continue
            # resync: skip reads on either side (polling loops differ in length)
            if ka == 'R':
                i += 1; continue
            if kb == 'R':
                j += 1; continue
            print('PATH  #%d/%d A=%s %s  B=%s %s' % (i, j, ka, na, kb, nb)); shown += 1
            # look ahead for the next common write
            fa = {(x[1], x[2], x[3]): ii for ii, x in enumerate(a[i:i + 4000]) if x[1] == 'W'}
            for jj, x in enumerate(b[j:j + 4000]):
                if x[1] == 'W' and (x[1], x[2], x[3]) in fa:
                    i += fa[(x[1], x[2], x[3])]; j += jj; break
            else:
                break
        print('read mismatch counts:', reads, ' ops A %d B %d, stopped at %d/%d' % (len(a), len(b), i, j))
    elif cmd == 'awe':
        hz = float(sys.argv[4]) if len(sys.argv) > 4 else 400e6
        ops = load(sys.argv[2])
        out = open(sys.argv[3], 'w')
        out.write('ram 512\nrom $AWEROM\n')
        t0, fr = ops[0][0], 0
        for t, k, off, v in ops:
            f = int((t - t0) * 44100 / hz)
            if f > fr:
                out.write('wait %d\n' % (f - fr)); fr = f
            if k == 'W':
                out.write('io 0x%X 0x%X\n' % (off, v))
            elif k == 'R':
                out.write('ior 0x%X\n' % off)
        out.close()


main()
