#!/bin/bash
# duke.sh NAME SBEMU.EXE himem|jemm [SECS] : 86Box P2B-LS PII-400 (p2bls-uhci.cfg, UHCI + one emulated 360 pad), MS-DOS 7.1
# (store-dos71), HDPMI32I + SBEMU /AWE:620 /AWERAM:1024 /JOY:1 (HIMEM only, or JEMMEX NOEMS + JLOAD QPIEMU), SBDIAG START,
# Duke 3D 1.3D with AWE32 music (DUKE3D.CFG) for SECS (default 60), quit, SBDIAG -> /tmp/duke-NAME/{SB,DIAG}.TXT, aud/*.wav.
# Needs: store-dos71, /home/claude/rel (unzipped release), Duke unpacked in /tmp/duke/D, 86Box per ../86box/README.md.
# Run from sc55-pack (beta.121-awe.1). Compare two runs: python3 testrig/awe/cmp.py A.wav 5 55 B.wav 5 55
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H); R=/home/claude/rel/NUKEDSB/FILES
N=$1; EXE=$2; MODE=$3; SECS=${4:-60}; V=/tmp/duke-$N; rm -rf $V; mkdir -p $V/c/SC55/ROMS $V/c/DUKE
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
cp $R/HDPMI/HDPMI32I.EXE $R/JEMM/JEMMEX.EXE $R/JEMM/JLOAD.EXE $R/JEMM/QPIEMU.DLL $V/c/SC55/
cp $EXE $V/c/SC55/SBEMU.EXE; cp ${SBDIAG:-$TR/../output/sbdiag.exe} $V/c/SC55/SBDIAG.EXE
cp /home/claude/86box/roms/sound/creative/awe32.raw $V/c/SC55/ROMS/AWE32.RAW
cp /tmp/duke/D/* $V/c/DUKE/; cp $H/DUKE3D.CFG $V/c/DUKE/
if [ $MODE = jemm ]; then C='DEVICE=C:\SC55\JEMMEX.EXE NOEMS'; Q='C:\SC55\JLOAD C:\SC55\QPIEMU.DLL'; else C='DEVICE=C:\DOS\HIMEM.SYS'; Q=REM; fi
printf '%s\r\nDOS=HIGH\r\nFILES=40\r\n' "$C" > $V/c/CONFIG.SYS
printf "@ECHO OFF\r\nPATH C:\\\\DOS\r\n$Q\r\nC:\\\\SC55\\\\HDPMI32I -r\r\nCD \\\\SC55\r\nSBEMU /A220 /I5 /D1 /H5 /T6 /AWE:620 /AWERAM:1024 /JOY:1 > C:\\\\SB.TXT\r\nSBDIAG START > NUL\r\nCD \\\\DUKE\r\nDUKE3D\r\nC:\\\\SC55\\\\SBDIAG > C:\\\\DIAG.TXT\r\nECHO DONE > C:\\\\DONE.TXT\r\n" > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
echo 'lx=0 ly=0 rx=0 ry=0 lt=0 rt=0 buttons=0' > $V/xpad1.st; echo "export XPAD=1 XPAD_STATE=$V/xpad1.st" > $V/env
bash $TR/86box/run.sh $V > /dev/null
QUIT=1 python3 $H/drv.py $V $SECS > $V/drv.log 2>&1 || true
# shareware pages after quit, then wait for SBDIAG
for i in $(seq 8); do python3 $TR/86box/a.py $V/a.sock 'key down 0x39' 'step 150' 'key up 0x39' 'step 1500' > /dev/null 2>&1 || sleep 1; done
python3 $TR/86box/a.py $V/a.sock release > /dev/null 2>&1 || true
for i in $(seq 30); do sleep 2; mtype -i $V/hd.img@@32256 ::/DONE.TXT > /dev/null 2>&1 && break; done
pkill -f "86Box.*$V" || true; sleep 1
for f in SB DIAG; do mtype -i $V/hd.img@@32256 ::/$f.TXT 2>/dev/null | tr -d '\r' > $V/$f.TXT || true; done
grep -aiE 'awe|joy|usb|trap|error|fail' $V/SB.TXT | head -8
grep -a "interrupts (\|uses .*CPU\|needs .*real time\|ticks" $V/DIAG.TXT || echo "no SBDIAG report"
