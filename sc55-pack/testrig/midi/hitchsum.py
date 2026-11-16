#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""hitchsum.py DBGLOG.LOG - beta.128: summary of SBEMU /DBG "usbmidi" stats lines (one per second) + the sound side:
   seconds, transfers/s median/max, bmax, queue max, latency avg/max, wait max, gap max, paced, nak, retry, poll max."""
import re, sys, statistics as st
L = open(sys.argv[1], errors='replace').read().splitlines()
u = [dict((k, v) for k, v in re.findall(r'(\w+)=([\d/]+)', l)) for l in L if 'usbmidi ' in l]
s = [l for l in L if ' load=' in l]
if not u: print('no usbmidi lines (%d stats lines) FAIL' % len(s)); sys.exit(1)
def col(k, i=0): return [int(x[k].split('/')[i]) for x in u if k in x]
def mx(k, i=0): v = col(k, i); return max(v) if v else 0
tx = col('tx')
print('secs=%d tx/s med=%d max=%d bmax=%d qmax=%d lat avg=%dus max=%dus wait=%dus gap=%dus paced=%d nak=%d retry=%d poll avg=%dus max=%dus' % (
    len(u), st.median(tx), max(tx), mx('bmax'), mx('q', 1), st.mean(col('lat')), mx('lat', 1), mx('wait'), mx('gap'),
    sum(col('paced')), sum(col('nak')), sum(col('retry')), st.mean(col('poll')), mx('poll', 1)))
isr = [int(m.group(1)) for l in s for m in [re.search(r' isr=(\d+)', l)] if m]
ig = [int(m.group(1)) for l in s for m in [re.search(r' igap=(\d+)', l)] if m]
if isr: print('sound isr/s med=%d igap max=%dus stats lines=%d' % (st.median(isr), max(ig) if ig else -1, len(s)))
