#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# hmi98.sh ZIP [VXD] [CASE] - HMI SOS MPU-401 code (Descent's) in a Win98 DOS box, QEMU acpi VM (store-w98qi-qemu), MPU ->
# AWE32 MIDI Synth (/AWE:620 /AWEMPU:GM, [AWE32 MIDI] Synth=1), debug link COM1:1 -> DBGLOG.PY (/tmp/w98dbg/hmi.log).
#   CASE hmi (default): HMIMPU.COM 3 (init, 64 bytes, reset) -> D:\R\H.TXT; w= = loops after FFh (real MPU: 1)
#   CASE midplay: MIDPLAY /Q:40 TEST.MID (MPU reset/ACK handshake, B0h panel), then HMIMPU 1
#   CASE desc: Descent 1.4 shareware (/tmp/g/descent: desc14sw.exe -> ARJ, 4GWBIND in the VM) DESCENT.CFG SB Pro 240/7/3
#        + MPU 330, started RUNS times (default 3; DARGS = its arguments); desckeys.sh plays PLAY s (120) each and quits
#        to DOS; S lines give mpu=/s, a hang = no S lines
# VXD = an SC55W.VXD to test instead of the zip's.
ZIP=$1; VXD=$2; CASE=${3:-hmi}; VM=acpi
H=$(cd "$(dirname "$0")" && pwd); P=$H/../..; R98=$H/../w98par/run.sh; W=/tmp/w98t; L=/tmp/w98dbg; PORT=${PORT:-4560}
mkdir -p $W $L && rm -rf $W/d && mkdir -p $W/d/T $W/d/R $W/u
rm -rf $W/u/* && unzip -q -o "$ZIP" -d $W/u && mv $W/u/NUKEDSB $W/d/PACK
mkdir -p $W/d/PACK/FILES/ROMS && cp /tmp/roms/roms-6.0/sound/creative/awe32.raw $W/d/PACK/FILES/ROMS/AWE32.RAW
[ -n "$VXD" ] && cp "$VXD" $W/d/PACK/FILES/WIN98/SC55W.VXD
cp $H/HMIMPU.COM $W/d/T/
S='C:\SC55\WIN98\SC55W.EXE'
w() { echo "CHOICE /T:Y,$1 /C:Y >NUL"; }
case $CASE in
hmi) { echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 30; echo 'D:\T\HMIMPU.COM 3 > D:\R\H.TXT'; w 5
       echo 'D:\T\HMIMPU.COM 2 >> D:\R\H.TXT'; w 5; echo 'D:\T\HMIMPU.COM S >> D:\R\H.TXT'; w 5; echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TH.BAT ;;
midplay) cp $H/TEST.MID $W/d/T/
     { echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 30; echo 'D:\PACK\FILES\SC55\MIDPLAY.EXE /Q:40 D:\T\TEST.MID > D:\R\M.TXT'
       echo 'ECHO midplay ran> D:\R\H.TXT'; w 5; echo 'D:\T\HMIMPU.COM 1 >> D:\R\H.TXT'; w 3; echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TH.BAT ;;
desc) cp -r /tmp/g/descent $W/d/T/DESCENT
      printf 'DigiDeviceID=0xe001\r\nDigiPort=0x240\r\nDigiIrq=7\r\nDigiDma=3\r\nMidiDeviceID=0xa001\r\nMidiPort=0x330\r\nDigiVolume=8\r\nMidiVolume=8\r\n' > $W/d/T/DESCENT/DESCENT.CFG
      { echo '@ECHO OFF'; echo "START $S /DBG:COM1:1"; w 30; echo 'D:'; echo 'CD \T\DESCENT'
        echo 'IF NOT EXIST DCNTSHR.EXE 4GWBIND 4GWPRO.EXE DESCENT.UBN DCNTSHR.EXE -Q > NUL'; echo 'ECHO bound> D:\R\B.TXT'
        for i in $(seq ${RUNS:-3}); do echo "DCNTSHR.EXE $DARGS"; echo "ECHO run $i>> D:\\R\\B.TXT"; done
        echo 'ECHO done> D:\R\DONE.TXT'; } | sed 's/$/\r/' > $W/d/T/TH.BAT ;;
esac
export QEXTRA="-chardev socket,id=c1,host=127.0.0.1,port=$PORT,server=on,wait=off -serial chardev:c1"
rm -f $L/hmi.log $L/hmi.err
sh $R98 fresh $VM && sh $R98 d > /dev/null
echo "== install: $(BOOT=${BOOT:-200} sh $R98 install $VM | tr '\n' ' ')"
I=$W/$VM.img@@32256
printf '/A:240 /I:7 /D:3 /H:7 /T:6 /P:330 /OPL:0 /AWE:620 /AWERAM:4096 /AWEMPU:GM\r\n' > $W/o.txt; mcopy -o -i $I $W/o.txt ::/SC55/SBOPTS.TXT
mtype -i $I ::/SC55/WIN98/SC55W.INI > $W/i.ini; printf '[AWE32 MIDI]\r\nSynth=1\r\nBank=GM\r\n' >> $W/i.ini; mcopy -o -i $I $W/i.ini ::/SC55/WIN98/SC55W.INI
( for i in $(seq 120); do [ -f $L/stop ] && break; python3 -I $P/pack/FILES/SC55/DBGLOG.PY socket://127.0.0.1:$PORT -o $L/hmi.log -q \
    --timeout 1500 2>>$L/hmi.err && break; sleep 2; done ) &
[ $CASE = desc ] && setsid sh $H/desckeys.sh $L/hmi.log ${RUNS:-3} ${PLAY:-120} > $L/keys.log 2>&1 < /dev/null &
BOOT=${BOOT:-200} sh $R98 t $VM TH.BAT ${SECS:-$([ $CASE = desc ] && echo 1500 || echo 360)} > /dev/null
pkill -f desckeys.sh
touch $L/stop; pkill -f "DBGLOG.PY socket://127.0.0.1:$PORT"; sleep 5; rm -f $L/stop
r=$W/r-$VM-TH
echo "results: $(tr -d '\r' < $r/H.TXT 2>/dev/null | tr '\n' '|') done=$([ -f $r/DONE.TXT ] && echo yes || echo NO)"
echo "log: $(grep -vc '^#' $L/hmi.log) lines; max mpu/s: $(grep -o 'mpu=[0-9]*' $L/hmi.log | cut -d= -f2 | sort -n | tail -1)"
grep -E "MPU-401|VXD E" $L/hmi.log | tail -8
[ $CASE = desc ] && { echo "desc: $(grep -c 'runs .*DCNTSHR' $L/hmi.log) starts, $(grep -c 'program ends' $L/hmi.log) program ends, $(tr -d '\r' < $r/B.TXT 2>/dev/null | tr '\n' ' ')"
  echo "  MPU resets $(grep -c 'MPU-401 reset' $L/hmi.log), spins $(grep -c 'times in a row' $L/hmi.log), S lines with midi>0: $(grep -cE 'VXD S vm=.* midi=[1-9]' $L/hmi.log), max midi/s $(grep -o ' midi=[0-9]*' $L/hmi.log | cut -d= -f2 | sort -n | tail -1)"
  echo "  AWE32 MIDI Synth: $(grep -o 'AWE32 MIDI Synth: [0-9]* bytes, [0-9]* notes' $L/hmi.log | tail -1)"; }
