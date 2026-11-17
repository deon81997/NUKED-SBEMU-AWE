# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line, awe.3)
# mkrel.py OLD.zip NEW.zip (repo root, after make + win98/buildall.sh): rezip.py (DOS EXEs, README.TXT, BATs), then the
# Windows 98 part from sc55-pack/win98/out, both README98.TXT copies and a rebuilt FILES/WIN98/SOURCE.ZIP (old file
# list with current files + the AWE engine and helper/awe.*). Run with -I.
# awe.9: --keepdos also swaps README.TXT + FILES/SC55/DBGLOG.PY/.TXT; SOURCE.ZIP gets dbg98.c + dbgtest.
# awe.5: --keepdos = no DOS change: the old zip's DOS files as they are (no rezip.py / make), HWTEST.TXT swapped.
import zipfile, os, sys, subprocess, io, shutil
keepdos = '--keepdos' in sys.argv; args = [a for a in sys.argv[1:] if a != '--keepdos']
old, new = args[0], args[1]; tmp = new + '.tmp'
if keepdos: shutil.copy(old, tmp)
else: subprocess.check_call([sys.executable, '-I', 'sc55-pack/testrig/common/rezip.py', old, tmp])
W = 'sc55-pack/win98/'; O = W + 'out/'
swap = {'NUKEDSB/FILES/WIN98/' + n: O + n for n in ('SC55W.EXE', 'SC55W2.EXE', 'SC55WM.EXE', 'SC55W.VXD', 'SC55MIDI.DRV')}
swap['NUKEDSB/FILES/WIN98/JOY/NUKEDJOY.VXD'] = O + 'JOY/NUKEDJOY.VXD'
swap['NUKEDSB/FILES/WIN98/README98.TXT'] = swap['NUKEDSB/README98.TXT'] = 'sc55-pack/pack/README98.TXT'
if keepdos: swap['NUKEDSB/HWTEST.TXT'] = 'sc55-pack/pack/HWTEST.TXT'
if keepdos: swap['NUKEDSB/README.TXT'] = 'sc55-pack/pack/README.TXT'   # awe.9: README.TXT, DBGLOG.* (w98.9 debug link)
for n in ('DBGLOG.PY', 'DBGLOG.TXT'): swap['NUKEDSB/FILES/SC55/' + n] = 'sc55-pack/pack/FILES/SC55/' + n
z = zipfile.ZipFile(tmp); w = zipfile.ZipFile(new, 'w', zipfile.ZIP_DEFLATED, compresslevel=9); n = 0
for i in z.infolist():
    if i.filename in swap: w.writestr(i, open(swap[i.filename], 'rb').read()); n += 1
    elif i.filename == 'NUKEDSB/FILES/WIN98/SOURCE.ZIP':
        sz = zipfile.ZipFile(io.BytesIO(z.read(i.filename))); b = io.BytesIO()
        out = zipfile.ZipFile(b, 'w', zipfile.ZIP_DEFLATED, compresslevel=9)
        names = [e.filename for e in sz.infolist() if not e.filename.endswith('/')]
        names += ['win98/helper/awe.cpp', 'win98/helper/awe.h', 'awe/synth/emu8k.c', 'awe/synth/emu8k.h', 'awe/synth/emu8k_rom.c',
                  'awe/synth/awemidi.c', 'awe/synth/awemidi.h', 'awe/synth/awegm.h',   # awe.5: AWE32 MIDI Synth
                  'win98/vxd/dbg98.c', 'win98/test/dbgtest.c', 'win98/test/dbgtest.sh']   # awe.9: serial debug link
        for f in dict.fromkeys(names):   # once each (a zip built by this script has the AWE files already)
            src = 'sc55-pack/' + f
            if os.path.isfile(src): out.write(src, f)
            else: out.writestr(f, sz.read(f))
        out.close(); w.writestr(i, b.getvalue()); n += 1
    else: w.writestr(i, z.read(i.filename))
w.close(); os.remove(tmp); print('win98 swapped', n)
