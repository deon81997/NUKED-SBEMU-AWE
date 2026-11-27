#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU
# jemm.sh TAG JEMMDIR [SBEMU.EXE] - JEMM check on the user's MS-DOS 7.1 (store-dos71), QEMU, one boot:
#   CONFIG.SYS = JEMMCFG.BAT's line (JEMMEX X2MAX=8192 NOEMS instead of HIMEM), the pack's SETUP C + CDINST C, SC55JEM.BAT
#   (JLOAD QPIEMU + HDPMI32I + SBEMU, SC-55 boot with the ROMs in $ROMS), RMTRAP.COM (real-mode DSP/MPU through
#   QPIEMU), CDON.BAT + UCDD -mount TEST.CUE + M:\TEST.TXT, SBDIAG. JEMMDIR = JEMMEX.EXE JLOAD.EXE QPIEMU.DLL
#   [JEMM386.EXE]; they replace the zip's FILES\JEMM. Result: /tmp/jt-TAG/RESULT.LOG (+ SETUP/JEM/CD logs).
# Env: REL (unzipped release, default /home/claude/rel/NUKEDSB), ROMS (SC-55 set, default mk2 from store-personal),
#      IMG (default /home/claude/vms/dos71/msdos71.img), TMO (default 600). Screenshot at the end: /tmp/jt-TAG/end.png.
set -u
T=$(cd "$(dirname "$0")" && pwd); TR=$(dirname "$T")
TAG=$1 JD=$2 SBX=${3:-}; REL=${REL:-/home/claude/rel/NUKEDSB}; IMG=${IMG:-/home/claude/vms/dos71/msdos71.img}
ROMS=${ROMS:-/home/claude/tmp/p52/NUKEDSB/FILES/ROMS}; W=/tmp/jt-$TAG
rm -rf "$W"; mkdir -p "$W/NUKEDSB"; cp -r "$REL"/. "$W/NUKEDSB/"
for f in JEMMEX.EXE JLOAD.EXE QPIEMU.DLL JEMM386.EXE; do [ -f "$JD/$f" ] && cp "$JD/$f" "$W/NUKEDSB/FILES/JEMM/$f"; done
[ -n "$SBX" ] && cp "$SBX" "$W/NUKEDSB/FILES/SC55/SBEMU.EXE"
cp "$ROMS"/M2_*.BIN "$W/NUKEDSB/FILES/ROMS/" 2>/dev/null
nasm -f bin -o "$W/RMTRAP.COM" "$T/rmtrap.asm"
cp --sparse=always "$IMG" "$W/c.img"
OFF=$(python3 -c "import sys;b=open(sys.argv[1],'rb').read(512);print(int.from_bytes(b[454:458],'little')*512)" "$W/c.img")
M="$W/c.img@@$OFF"
printf 'DEVICE=C:\\NUKEDSB\\FILES\\JEMM\\JEMMEX.EXE X2MAX=8192 NOEMS\r\nDOS=HIGH,UMB\r\nFILES=40\r\nBUFFERS=20\r\nLASTDRIVE=Z\r\n' > "$W/CONFIG.SYS"
mcopy -o -i "$M" "$W/CONFIG.SYS" ::CONFIG.SYS; mcopy -i "$M" "$W/RMTRAP.COM" ::RMTRAP.COM; mcopy -s -i "$M" "$W/NUKEDSB" ::
cat > "$W/JT.BAT" <<'EOF'
@ECHO OFF
VER > C:\RESULT.LOG
C:\NUKEDSB\FILES\JEMM\JEMMEX.EXE > C:\JV.TXT
FIND /I "JemmEx v" C:\JV.TXT >> C:\RESULT.LOG
IF ERRORLEVEL 1 GOTO NOJEMM
MEM | FIND "Upper" >> C:\RESULT.LOG
CD \NUKEDSB
CALL SETUP.BAT C > C:\SETUP.LOG
CD \NUKEDSB
CALL CDINST.BAT C >> C:\SETUP.LOG
C:
CD \
IF NOT EXIST C:\SC55\SC55JEM.BAT GOTO NOSETUP
IF NOT EXIST C:\SC55\RM\JEMM\QPIEMU.DLL GOTO NOSETUP
ECHO SETUP OK >> C:\RESULT.LOG
CALL C:\SC55\SC55JEM.BAT > C:\JEM.LOG
C:
CD \
C:\RMTRAP.COM >> C:\RESULT.LOG
IF ERRORLEVEL 1 GOTO FAIL
ECHO STEP CDON >> C:\RESULT.LOG
CALL C:\CDSET\CDON.BAT > C:\CD.LOG
ECHO STEP MOUNT >> C:\RESULT.LOG
C:\CDSET\UCDD.EXE -mount C:\CDIMAGES\TEST.CUE >> C:\CD.LOG
IF NOT EXIST M:\TEST.TXT GOTO NOCD
ECHO M: MOUNT OK >> C:\RESULT.LOG
TYPE M:\TEST.TXT >> C:\RESULT.LOG
C:\SC55\SBDIAG.EXE >> C:\RESULT.LOG
ECHO PASS >> C:\RESULT.LOG
GOTO END
:NOJEMM
ECHO FAIL JEMMEX not loaded >> C:\RESULT.LOG
GOTO END
:NOSETUP
ECHO FAIL setup >> C:\RESULT.LOG
GOTO END
:NOCD
ECHO FAIL M: not mounted >> C:\RESULT.LOG
GOTO END
:FAIL
ECHO FAIL >> C:\RESULT.LOG
:END
EOF
sed -i 's/$/\r/' "$W/JT.BAT"
"$TR/common/run.sh" -i -t "${TMO:-600}" -s "$W/end.png" -k "$W/c.img" "$W/JT.BAT" > "$W/RESULT.LOG" 2> "$W/run.err"; rc=$?
for f in JV.TXT SETUP.LOG JEM.LOG CD.LOG SC55/SBLOG.TXT; do mtype -i "$M" "::$f" > "$W/$(basename $f)" 2>/dev/null; done
cat "$W/RESULT.LOG"; exit $rc
