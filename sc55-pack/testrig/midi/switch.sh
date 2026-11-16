#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# switch.sh CASE - MIDI output switch without a restart (beta.123), QEMU + the fake Uno (and a second fake device),
# SoundFont TimGM6mb as the emulated synth (S=/tmp/scr needs TIM.SF2 + mkswitch.py scripts + MPUPLAY.EXE).
#   rerun   the user's way: SC55 with the emulated synth, SETTINGS = USB MIDI (SBOPTS /MUSB:1), SC55 again, back again
#   mout    two USB MIDI devices: SBEMU /MOUT:USB1, USB2, NEXT (-> synth), USB1
#   env SBX: extra options for the first SBEMU (AWE line: SBX="/AWE:620 ").
#   hotkey  two devices, Ctrl+Alt+F11 four times during a song (synth -> USB 1 -> USB 2 -> synth -> USB 1)
#   loud    (beta.128) one device, quiet GS set-up (SWQ) + 110 s of notes (SWL), Ctrl+Alt+F11 x6 (synth <-> USB 1);
#   midplay (beta.128) the same with MIDPLAY /DBG:COM1 playing genmid.py's 16-channel song (ui log on COM1 -> file):
#           loudcheck.py + MIDPLAY must never see Ctrl+Alt+F11 (no "key 018B" in its log)
#           also loudcheck.py: both outputs' streams, no note at a wrong volume / program, nothing held, reset gaps
# Every switch to USB must replay every channel's program; what each device got must equal the game's stream through the
# reference MPU-401 + SBEMU's own silence / replay bytes (midicap.py). Last line PASS/FAIL.
H=$(cd "$(dirname "$0")" && pwd); S=${S:-/tmp/scr}; C=$1; HC=uhci; K=""; F2=""
printf '/VSF:C:\\T\\TIM.SF2 /MUSB:0\r\n' > $S/OPT0.TXT; printf '/VSF:C:\\T\\TIM.SF2 /MUSB:1\r\n' > $S/OPT1.TXT
P='C:\T\MPUPLAY C:\T\'
case $C in
  rerun)  RUN="SBEMU /VMPU /JOY:1 ${SBX:-}/MIDICAP:1 @C:\\T\\OPT0.TXT|${P}SWA.TXT > C:\\OUT.TXT|SBEMU /VMPU @C:\\T\\OPT1.TXT >> C:\\OUT.TXT|${P}SWB.TXT >> C:\\OUT.TXT|SBEMU /VMPU @C:\\T\\OPT0.TXT >> C:\\OUT.TXT|${P}SWC.TXT >> C:\\OUT.TXT|SBEMU /VMPU @C:\\T\\OPT1.TXT >> C:\\OUT.TXT|${P}SWD.TXT >> C:\\OUT.TXT|SC55MENU C:\\SC55 /STATUS >> C:\\OUT.TXT"; N=4 ;;
  mout)   HC=xhci; F2=1; RUN="SBEMU /VMPU /JOY:1 ${SBX:-}/VSF:C:\\T\\TIM.SF2 /MIDICAP:1|${P}SWA.TXT > C:\\OUT.TXT|SBEMU /MOUT:USB1 >> C:\\OUT.TXT|${P}SWB.TXT >> C:\\OUT.TXT|SBEMU /MOUT:USB2 >> C:\\OUT.TXT|${P}SWC.TXT >> C:\\OUT.TXT|SBEMU /MOUT:NEXT >> C:\\OUT.TXT|${P}SWD.TXT >> C:\\OUT.TXT|SBEMU /MOUT:USB1 >> C:\\OUT.TXT|${P}SWB.TXT >> C:\\OUT.TXT|SBDIAG USB >> C:\\OUT.TXT"; N=5 ;;
  hotkey) F2=1; K="--keys 45:ctrl-alt-f11+300,58:ctrl-alt-f11+300,71:ctrl-alt-f11+300,84:ctrl-alt-f11+300"
          RUN="SBEMU /VMPU /JOY:1 ${SBX:-}/VSF:C:\\T\\TIM.SF2 /MIDICAP:1|${P}SWA.TXT > C:\\OUT.TXT|${P}SWS.TXT >> C:\\OUT.TXT|${P}SWS.TXT >> C:\\OUT.TXT"; N=5 ;;
  loud)   K="--keys 30:ctrl-alt-f11+300,40:ctrl-alt-f11+300,50:ctrl-alt-f11+300,60:ctrl-alt-f11+300,70:ctrl-alt-f11+300,80:ctrl-alt-f11+300"
          RUN="SBEMU /VMPU /JOY:1 ${SBX:-}/VSF:C:\\T\\TIM.SF2 /MIDICAP:1|${P}SWQ.TXT > C:\\OUT.TXT|${P}SWL.TXT >> C:\\OUT.TXT|SBDIAG USB >> C:\\OUT.TXT"; N=6 ;;
  midplay) K="--keys 35:ctrl-alt-f11+300,45:ctrl-alt-f11+300,55:ctrl-alt-f11+300,65:ctrl-alt-f11+300,75:ctrl-alt-f11+300,85:ctrl-alt-f11+300"
          [ -f $S/SONG.MID ] || python3 "$H/../midplay/genmid.py" $S/SONG.MID 120
          export RIGCOM1FILE=/tmp/switch-midplay-com1.txt; rm -f $RIGCOM1FILE
          RUN="SBEMU /VMPU /JOY:1 ${SBX:-}/VSF:C:\\T\\TIM.SF2 /MIDICAP:1|MIDPLAY /DBG:COM1 /Q:90 C:\\T\\SONG.MID > C:\\OUT.TXT|SBDIAG USB >> C:\\OUT.TXT"; N=6 ;;
  *) echo "unknown case FAIL"; exit 2 ;;
esac
D=/tmp/switch-$C$RIGPORT.mem; rm -f $D
FAKEMIDI2=$F2 RIGTAG=-sw$C python3 "$H/rig.py" $HC "$RUN" ${SECS:-160} --dir "$S:T" --dump $D --mem 64 $K > /dev/null 2>&1
L="$H/fakemidi-$HC-sw$C.log"; [ -n "$F2" ] && L="$L $H/fakemidi-$HC-sw$C-2.log"
r=$(python3 "$H/midicap.py" $D $L)
if [ $C = loud ] || [ $C = midplay ]; then lc=$(python3 "$H/loudcheck.py" $D); echo "$lc"; [ $C = midplay ] && { k=$(grep -c "MIDPLAY key 018B\\|MIDPLAY key 0189" $RIGCOM1FILE); l=$(grep -c "song 0" $RIGCOM1FILE); g=$(grep -c "hotkey .* ignored" $RIGCOM1FILE); echo "MIDPLAY log (COM1): $k Ctrl+Alt+F11 keys acted on by MIDPLAY, $g seen and ignored"; [ "$k" = 0 ] && [ "$l" -gt 0 ] || lc="$lc
loudcheck: FAIL (MIDPLAY got the hotkey)"; }; echo "$lc" | tail -n 1 | grep -q PASS || { [ -n "$KEEP" ] && cp $D /tmp/switch-$C.keep; rm -f $D; echo "switch.sh $C: FAIL"; exit 1; }; fi
[ -n "$KEEP" ] && cp $D /tmp/switch-$C.keep; rm -f $D
echo "$r" | grep -v "^    "
n=$(echo "$r" | sed -n 's/.*USB packets, \([0-9]*\) output switches.*/\1/p')
[ "$n" -ge "$N" ] 2>/dev/null || { echo "switch.sh $C: $n output switches, at least $N expected FAIL"; exit 1; }
[ $C = rerun ] && { tr -d '\r' < "$H/out-$HC-sw$C.txt" | grep "MIDI output"; }
echo "$r" | tail -n 1 | grep -q PASS && echo "switch.sh $C: PASS" || { echo "switch.sh $C: FAIL"; exit 1; }
