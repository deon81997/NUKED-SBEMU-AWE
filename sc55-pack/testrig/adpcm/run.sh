#!/bin/bash
# run.sh ZIP [EXE...] - ADPCM DMA test (beta.133, upstream 11d5924): QEMU MS-DOS 7.1 image from the release zip (+ new EXEs),
# SETUP C + SC55.BAT, then ADPCMT.EXE twice (tiny 1-8 byte ADPCM blocks crashed SBEMU before). Prints the log; PASS/FAIL last.
H=$(cd "$(dirname "$0")" && pwd); W=/tmp/adpcmtest; ZIP=$1; shift
bash "$H/../menustat/mkimg.sh" "$ZIP" "$W" "$@" > /dev/null || exit 2
QAUDIO=${QAUDIO:-none,id=snd,timer-period=250} bash "$H/../common/run.sh" -t 200 -p "$H/ADPCMT.EXE" "$W/pm.img" "$H/ADPCM.BAT"
