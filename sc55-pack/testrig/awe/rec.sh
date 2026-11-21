#!/bin/bash
# rec.sh GAME SBEMU.EXE [SECS] : record the EMU8000 port stream of a game set to AWE32 music (beta.131-awe.2).
# GAME = duke | rott | heretic | descent (/tmp/games/desc: cap.sh's unpack; title music). 86Box P2B-LS (duke.sh's VM), MS-DOS 7.1 HIMEM, HDPMI32I + an AWEREC SBEMU build
# (make AWEREC=1: sbemu/vawe.c logs every EMU8000 word access with the TSC), game for SECS (default 70) of emulated
# time in its demo/attract mode, quit keys, SBDIAG FLUSH writes C:\AWEREC.BIN -> /tmp/rec-GAME/AWEREC.BIN, plus
# SBEMU's own output in aud/main.wav. Replay: awe/synth/test/harness -s AWEREC.BIN (ours) / harness_ref (86Box).
# Needs what duke.sh needs, plus ROTT in /tmp/rott/R (unzip ROTTSW13.SHR), Heretic in /tmp/htic/H
# (cat HTIC_V12.1 HTIC_V12.2 > x.exe; unzip x.exe - a PKZIP SFX).
# NATIVE=1: no SBEMU - 86Box's own AWE64 (1 MB) with the AWEREC86 logger patch (priavte testrig/awenative), same
# AWEREC1 format into /tmp/rec-GAME-86/AWEREC.BIN: what the game's driver does on a "real" chip.
set -e
H=$(cd $(dirname $0); pwd); TR=$(dirname $H); R=/home/claude/rel/NUKEDSB/FILES
G=$1; EXE=$2; SECS=${3:-70}; V=/tmp/rec-$G${NATIVE:+-86}; rm -rf $V; mkdir -p $V/c/SC55/ROMS $V/c/GAME
cp --sparse=always /home/claude/vms/dos71/msdos71.img $V/hd.img
cp $R/HDPMI/HDPMI32I.EXE $V/c/SC55/; cp $EXE $V/c/SC55/SBEMU.EXE; cp ${SBDIAG:-$TR/../output/sbdiag.exe} $V/c/SC55/SBDIAG.EXE
cp /home/claude/86box/roms/sound/creative/awe32.raw $V/c/SC55/ROMS/AWE32.RAW
case $G in
  duke) cp /tmp/duke/D/* $V/c/GAME/; cp $H/DUKE3D.CFG $V/c/GAME/; RUN=DUKE3D ;;
  rott) cp /tmp/rott/R/* $V/c/GAME/; cp $H/SOUND.ROT $V/c/GAME/; RUN=ROTT ;;
  heretic) cp /tmp/htic/H/* $V/c/GAME/; cp $H/HERETIC.CFG $V/c/GAME/; RUN=HERETIC ;;
  descent) for f in /tmp/games/desc/*; do case $(basename $f) in *.SOW|*.ARJ|*.A01|*.exe|INSTALL.EXE) ;; *) cp $f $V/c/GAME/ ;; esac; done
        printf 'MidiDeviceID=0xa008\r\nMidiPort=0x620\r\nMidiVolume=100\r\n' > $V/c/GAME/DESCENT.CFG
        RUN='4GWBIND 4GWPRO.EXE DESCENT.UBN DCNTSHR.EXE -Q > NUL\r\nDCNTSHR' ;;
esac
printf 'DEVICE=C:\\DOS\\HIMEM.SYS\r\nDOS=HIGH\r\nFILES=40\r\n' > $V/c/CONFIG.SYS
SB="C:\\\\SC55\\\\HDPMI32I -r\r\nCD \\\\SC55\r\nSBEMU ${OPTS:-/A220 /I5 /D1 /H5 /T6 /AWE:620 /AWERAM:1024} > C:\\\\SB.TXT"
[ -n "${NATIVE:-}" ] && SB="SET BLASTER=A220 I5 D1 H5 P330 T6 E620"
printf "@ECHO OFF\r\nPATH C:\\\\DOS\r\n$SB\r\nCD \\\\GAME\r\n$RUN\r\nC:\\\\SC55\\\\SBDIAG FLUSH > C:\\\\FL.TXT\r\nECHO DONE > C:\\\\DONE.TXT\r\n" > $V/c/AUTOEXEC.BAT
mcopy -o -s -i $V/hd.img@@32256 $V/c/* ::/
sed -e 's#^hdd_01_fn = .*#hdd_01_fn = hd.img#' -e 's/^hdd_01_parameters = .*/hdd_01_parameters = 63, 16, 2080, 0, ide/' $TR/86box/p2bls-uhci.cfg > $V/86box.cfg
echo 'lx=0 ly=0 rx=0 ry=0 lt=0 rt=0 buttons=0' > $V/xpad1.st; echo "export XPAD=1 XPAD_STATE=$V/xpad1.st" > $V/env
if [ -n "${NATIVE:-}" ]; then
  sed -i 's/^sndcard = .*/sndcard = sbawe64/' $V/86box.cfg; printf '\n[Sound Blaster AWE64 #1]\nonboard_ram = 1024\n' >> $V/86box.cfg
  echo "export AWEREC86=$V/AWEREC.BIN" >> $V/env
fi
bash $TR/86box/run.sh $V > /dev/null
python3 $H/recdrv.py $V $SECS $G > $V/drv.log 2>&1 || true
[ -n "${NATIVE:-}" ] || python3 $H/recmem.py $V/a.sock $V/AWEREC.BIN || true   # copy now: ROTT never gets back to DOS
for i in $(seq 40); do sleep 3; mtype -i $V/hd.img@@32256 ::/DONE.TXT > /dev/null 2>&1 && break; python3 $TR/86box/a.py $V/a.sock 'key down 0x01' 'step 300' 'key up 0x01' 'step 1500' 'key down 0x39' 'step 300' 'key up 0x39' 'step 1500' > /dev/null 2>&1 || true; done
pkill -f "86Box.*$V" || true; sleep 1
[ -n "${NATIVE:-}" ] || [ -f $V/AWEREC.BIN ] || mcopy -o -i $V/hd.img@@32256 ::/AWEREC.BIN $V/ 2>/dev/null || echo "no AWEREC.BIN"
for f in SB FL; do mtype -i $V/hd.img@@32256 ::/$f.TXT 2>/dev/null | tr -d '\r' > $V/$f.TXT || true; done
grep -ai "awe" $V/SB.TXT | head -3; ls -l $V/AWEREC.BIN 2>/dev/null
