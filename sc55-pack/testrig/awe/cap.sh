#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (AWE line)
# cap.sh NAME sbemu|native duke|descent|heretic [SECS] : EMU8000 port stream capture on 86Box (P2B-LS PII-400, MS-DOS 7.1).
#   sbemu : ES1371 + HDPMI32I + SBEMU.EXE ($EXE, a VAWE_TRACE=1 build: every trapped EMU8000 byte goes out to port 5E0h)
#   native: 86Box's own Sound Blaster AWE32 (sbawe32, 220/5/1/5, EMU 620, 512 KB), no SBEMU
# Needs the capture 86Box (testrig/awe/86box-emu8ktrace.diff on top of ../86box/86box-xpad.diff): EMU8K_TRACE=file logs
# "tsc K off val" (W/R = word, w/r = byte; off from 620h). Out: /tmp/cap-NAME/{trace.txt,aud/*.wav,s*.png}.
# Games unpacked under $G (default /tmp/games): duke/D (DN3DSW13.SHR), desc (DESCENT.ARJ+.A01, arj x -v), htic/H.
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H); R=/home/claude/rel/NUKEDSB/FILES; G=${G:-/tmp/games}
N=$1; MODE=$2; GAME=$3; SECS=${4:-45}; V=/tmp/cap-$N; rm -rf $V; mkdir -p $V/c/SC55/ROMS $V/c/G
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
BL='SET BLASTER=A220 I5 D1 H5 P330 T6 E620'
case $GAME in
  duke) cp $G/duke/D/* $V/c/G/; cp ${CFG:-$H/DUKE3D.CFG} $V/c/G/DUKE3D.CFG; sed -i 's/^FXDevice = .*/FXDevice = 13/' $V/c/G/DUKE3D.CFG
        RUN='DUKE3D' ;;
  descent) for f in $G/desc/*; do case $(basename $f) in *.SOW|*.ARJ|*.A01|INSTALL.EXE) ;; *) cp $f $V/c/G/ ;; esac; done
        printf 'MidiDeviceID=0xa008\r\nMidiPort=0x620\r\nMidiVolume=100\r\n' > $V/c/G/DESCENT.CFG
        RUN='4GWBIND 4GWPRO.EXE DESCENT.UBN DCNTSHR.EXE -Q > NUL\r\nDCNTSHR' ;;
  heretic) cp $G/htic/H/* $V/c/G/
        printf 'snd_musicdevice 9\r\nsnd_sfxdevice 0\r\nsnd_mport 1568\r\nsnd_musicvolume 15\r\nsnd_sbport 544\r\nsnd_sbirq 5\r\nsnd_sbdma 1\r\n' > $V/c/G/HERETIC.CFG
        RUN='HERETIC' ;;
esac
printf 'DEVICE=C:\\DOS\\HIMEM.SYS\r\nDOS=HIGH\r\nFILES=40\r\n' > $V/c/CONFIG.SYS
if [ $MODE = sbemu ]; then
  cp $R/HDPMI/HDPMI32I.EXE $V/c/SC55/; cp ${EXE:-/tmp/bin/sbemu_trace.exe} $V/c/SC55/SBEMU.EXE
  cp /home/claude/86box/roms/sound/creative/awe32.raw $V/c/SC55/ROMS/AWE32.RAW
  PRE="C:\\\\SC55\\\\HDPMI32I -r\r\nCD \\\\SC55\r\nSBEMU ${OPTS:-/A220 /I5 /D1 /H5 /T6 /AWE:620 /AWERAM:512 /AWEMPU:0} > C:\\\\SB.TXT"
  SND=es1371
else
  PRE="$BL"; SND=sbawe32
fi
printf "@ECHO OFF\r\nPATH C:\\\\DOS\r\n$PRE\r\nCD \\\\G\r\n$RUN\r\nECHO DONE > C:\\\\DONE.TXT\r\n" > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' \
    -e "s/^sndcard = .*/sndcard = $SND/" $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
[ $SND = sbawe32 ] && printf '\n[Sound Blaster AWE32 #1]\nonboard_ram = 512\n' >> $V/86box.cfg
echo 'lx=0 ly=0 rx=0 ry=0 lt=0 rt=0 buttons=0' > $V/xpad1.st
echo "export XPAD=1 XPAD_STATE=$V/xpad1.st EMU8K_TRACE=$V/trace.txt" > $V/env
bash $TR/86box/run.sh $V > /dev/null
python3 $H/drv.py $V $SECS > $V/drv.log 2>&1 || true
pkill -f "86Box.*$V" || true; sleep 1
mcopy -o -i $V/hd.img@@32256 ::/SB.TXT $V/SB.TXT 2>/dev/null || true
echo "$N: $(wc -l < $V/trace.txt) trace lines"
