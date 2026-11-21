#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (awe.7)
# vmquit98.sh ZIP [VXD] [NAME] - a DOS box that ends while its Sound Blaster auto-init DMA still runs (a game crashing or
# being closed). QEMU acpi Win98 (store-w98qi-qemu, w98par/run.sh). SC55W at 240/7/3; TH.BAT: START SC55W, DMAQUIT.COM
# (dmaquit.asm: DSP 1Ch auto-init, exits without stopping it), 5 s of sound, DONE, the batch ends = the DOS box closes,
# twice more in new DOS boxes. Up to awe.6: blue screen "exception 0E ... VxD SC55W(01) + 0000178A" (convert() reading the
# closed VM's DMA buffer from the timer). Screenshots /tmp/w98t/shots/vq-NAME-*.png; prints whether Windows still runs.
ZIP=$1; VXD=$2; NAME=${3:-x}; VM=acpi
H=$(cd "$(dirname "$0")" && pwd); R98=$H/../w98par/run.sh; W=/tmp/w98t; Q="python3 /home/claude/vms/bin/q.py"
export QSOCK=$W/q.sock SHOTS=${SHOTS:-$W/shots}
mkdir -p $W $SHOTS && rm -rf $W/d $W/u && mkdir -p $W/d/T $W/d/R $W/u
unzip -q -o "$ZIP" -d $W/u && mv $W/u/NUKEDSB $W/d/PACK
[ -n "$VXD" ] && cp "$VXD" $W/d/PACK/FILES/WIN98/SC55W.VXD
cp $H/DMAQUIT.COM $W/d/T/
w() { echo "CHOICE /T:Y,$1 /C:Y >NUL"; }
{ echo '@ECHO OFF'; echo 'START C:\SC55\WIN98\SC55W.EXE'; w 30; echo 'D:\T\DMAQUIT.COM'; w 5; echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TH.BAT
{ echo '@ECHO OFF'; echo 'D:\T\DMAQUIT.COM'; w 5; } | sed 's/$/\r/' > $W/d/T/T2.BAT
sh $R98 fresh $VM && sh $R98 d > /dev/null
echo "== install: $(BOOT=${BOOT:-200} sh $R98 install $VM | tr '\n' ' ')"
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:330 /OPL:0\r\n' > $W/o.txt; mcopy -o -i $W/$VM.img@@32256 $W/o.txt ::/SC55/SBOPTS.TXT
# boot, run TH.BAT (closes its box after DONE), then two more boxes with T2.BAT; shots after each
pkill -f "qemu-system-i386.*$W/" 2>/dev/null; sleep 1
TESTIMG=$W/test.img SND=${SND:-ac97 sb16} setsid /home/claude/vms/bin/qrun.sh $W/$VM.img -no-reboot > $W/qemu.log 2>&1 < /dev/null &
sleep ${BOOT:-200}
rc() { $Q key esc; $Q key ctrl-esc; sleep 2; $Q key r; sleep 3; $Q type "$1"; $Q key ret; }
rc 'D:\T\TH.BAT'; n=0
while [ $n -lt 300 ]; do sleep 10; n=$((n + 10)); mtype -i $W/test.img@@32256 ::/R/DONE.TXT 2>/dev/null | grep -q done && break; done
sleep 15; $Q shot vq-$NAME-1 > /dev/null
rc 'D:\T\T2.BAT'; sleep 40; $Q shot vq-$NAME-2 > /dev/null
rc 'D:\T\T2.BAT'; sleep 40; $Q shot vq-$NAME-3 > /dev/null
rc 'NOTEPAD.EXE'; sleep 10; $Q shot vq-$NAME-4 > /dev/null
pgrep -f "qemu-system-i386.*$W/" > /dev/null && echo "QEMU running" || echo "QEMU gone (reboot/crash)"
pkill -f "qemu-system-i386.*$W/"; ls $SHOTS | grep "vq-$NAME"
