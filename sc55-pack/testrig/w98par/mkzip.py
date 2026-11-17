#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
"""mkzip.py OLD.zip NEW.zip - the Win98 release zip: OLD with the Windows 98 files swapped in (run from the repo root
after win98/buildall.sh). FILES/WIN98/SOURCE.ZIP is rebuilt from the win98 sources (its old file list + the new files).
Text files keep the line endings the old zip had for them."""
import io, os, sys, zipfile
old, new = sys.argv[1], sys.argv[2]
P = 'sc55-pack'
W = P + '/win98'
swap = {
    'NUKEDSB/FILES/WIN98/SC55W.EXE': W + '/out/SC55W.EXE', 'NUKEDSB/FILES/WIN98/SC55WM.EXE': W + '/out/SC55WM.EXE',
    'NUKEDSB/FILES/WIN98/SC55W2.EXE': W + '/out/SC55W2.EXE', 'NUKEDSB/FILES/WIN98/SC55W.VXD': W + '/out/SC55W.VXD',
    'NUKEDSB/FILES/WIN98/SC55MIDI.DRV': W + '/out/SC55MIDI.DRV', 'NUKEDSB/FILES/WIN98/JOY/NUKEDJOY.VXD': W + '/out/JOY/NUKEDJOY.VXD',
    'NUKEDSB/FILES/WIN98/README98.TXT': P + '/pack/README98.TXT', 'NUKEDSB/README98.TXT': P + '/pack/README98.TXT',
    'NUKEDSB/README.TXT': P + '/pack/README.TXT', 'NUKEDSB/HWTEST.TXT': P + '/pack/HWTEST.TXT',
    'NUKEDSB/FILES/SC55/BUGREP.BAT': P + '/pack/FILES/SC55/BUGREP.BAT',
    'NUKEDSB/FILES/SC55/DBGLOG.PY': P + '/pack/FILES/SC55/DBGLOG.PY', 'NUKEDSB/FILES/SC55/DBGLOG.TXT': P + '/pack/FILES/SC55/DBGLOG.TXT',
}
add_src = ['win98/VERSION', 'win98/common/nsbver.h', 'win98/helper/midiroute.cpp', 'win98/helper/midiroute.h',
           'win98/tools/mkver.sh', 'win98/test/routetest.cpp', 'win98/test/routetest.sh', 'win98/test/mpuint.c',
           'win98/test/mswitch.c', 'win98/test/w32shim/windows.h', 'win98/test/w32shim/mmsystem.h',
           'win98/test/w32shim/fakemidi.cpp', 'win98/vxd/dbg98.c', 'win98/test/dbgtest.c', 'win98/test/dbgtest.sh']
zi = zipfile.ZipFile(old)
def fix_eol(data, like):
    if b'\0' in data[:4096]: return data
    data = data.replace(b'\r\n', b'\n')
    return data.replace(b'\n', b'\r\n') if b'\r\n' in like else data
out = zipfile.ZipFile(new, 'w', zipfile.ZIP_DEFLATED, compresslevel=9)
for i in zi.infolist():
    d = zi.read(i.filename)
    if i.filename in swap:
        nd = open(swap[i.filename], 'rb').read()
        d = fix_eol(nd, d) if i.filename.upper().endswith(('.TXT', '.BAT')) else nd
    elif i.filename == 'NUKEDSB/FILES/WIN98/SOURCE.ZIP':
        s = zipfile.ZipFile(io.BytesIO(d))
        names = [n for n in s.namelist() if not n.startswith('win98/vxd/.dev')] + [n for n in add_src if n not in s.namelist()]
        b = io.BytesIO(); z2 = zipfile.ZipFile(b, 'w', zipfile.ZIP_DEFLATED, compresslevel=9)
        for n in names: z2.write(os.path.join(P, n), n)
        z2.close(); d = b.getvalue()
    j = zipfile.ZipInfo(i.filename, i.date_time); j.external_attr = i.external_attr; j.compress_type = zipfile.ZIP_DEFLATED
    out.writestr(j, d, compresslevel=9)
out.close()
print(new, os.path.getsize(new))
