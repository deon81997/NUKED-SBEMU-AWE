#!/usr/bin/env python3
"""rezip3p.py IN.ZIP OUT.ZIP [JEMMDIR] - third-party update pass after testrig/common/rezip.py (repo root, after make and
win98/buildall.sh): FILES/JEMM binaries from JEMMDIR (JemmB release: JEMMEX JEMM386 JLOAD QPIEMU.DLL) + its TXTs from
sc55-pack/pack/FILES/JEMM, FILES/DOSMODE/REALMODE.TXT, DOWNLOADS.TXT, README98.TXT (root + FILES/WIN98), the Win98
binaries from sc55-pack/win98/out and SOURCE.ZIP rebuilt with the same member list from the current tree."""
import io, os, sys, zipfile
src, dst = sys.argv[1:3]; jd = sys.argv[3] if len(sys.argv) > 3 else ''; P = 'sc55-pack/pack/'; W = 'sc55-pack/win98/out/'
m = {'NUKEDSB/DOWNLOADS.TXT': P + 'DOWNLOADS.TXT', 'NUKEDSB/README98.TXT': P + 'README98.TXT',
     'NUKEDSB/FILES/WIN98/README98.TXT': P + 'README98.TXT', 'NUKEDSB/FILES/DOSMODE/REALMODE.TXT': P + 'FILES/DOSMODE/REALMODE.TXT'}
if jd:  # optional: JEMM update (beta.134 skipped 5.87: hang, see HANDOFF)
    for f in ('JEMMEX.EXE', 'JEMM386.EXE', 'JLOAD.EXE', 'QPIEMU.DLL'): m['NUKEDSB/FILES/JEMM/' + f] = os.path.join(jd, f)
    for f in os.listdir(P + 'FILES/JEMM'): m['NUKEDSB/FILES/JEMM/' + f] = P + 'FILES/JEMM/' + f
for f in ('SC55W.EXE', 'SC55WM.EXE', 'SC55W2.EXE', 'SC55W.VXD', 'SC55MIDI.DRV', 'JOY/NUKEDJOY.VXD'): m['NUKEDSB/FILES/WIN98/' + f] = W + f
z = zipfile.ZipFile(src); o = zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED, compresslevel=9); n = 0
for i in z.infolist():
    if i.filename == 'NUKEDSB/FILES/WIN98/SOURCE.ZIP':
        old = zipfile.ZipFile(io.BytesIO(z.read(i))); b = io.BytesIO(); s = zipfile.ZipFile(b, 'w', zipfile.ZIP_DEFLATED, compresslevel=9)
        for j in old.infolist():
            p = 'sc55-pack/' + j.filename
            s.writestr(j, open(p, 'rb').read() if os.path.isfile(p) else old.read(j))
        s.close(); o.writestr(i, b.getvalue()); n += 1
    elif i.filename in m:
        o.writestr(i, open(m.pop(i.filename), 'rb').read()); n += 1
    else: o.writestr(i, z.read(i))
o.close()
print('swapped', n, 'not in the old zip (not added):', [k for k in m])
