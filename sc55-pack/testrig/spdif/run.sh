#!/bin/sh
# run.sh ZIP [EXE...] - /SPDIF + SBDIAG SPDIF smoke (QEMU MS-DOS 7.1, no Audigy). Expects, in order: start = analog + S/PDIF,
# re-run /SPDIF:ONLY = S/PDIF only, SBDIAG SPDIF 0 = analog only, bad /SPDIF:XYZ refused (stays analog only), all CARD=none.
#   sh testrig/spdif/run.sh ../release/NUKED-SB-EMU-*.zip output/sbemu.exe output/sbdiag.exe
H=$(cd "$(dirname "$0")" && pwd); W=/tmp/spdiftest
ZIP=$1; shift
sh "$H/../menustat/mkimg.sh" "$ZIP" "$W" "$@" || exit 2
got=$("$H/../common/run.sh" -t 150 "$W/pm.img" "$H/SPDIF.BAT" | grep "^S/PDIF: MODE=" | sed 's/ CARD=none.*//;s/S.PDIF: MODE=//' | tr '\n' '|')
want="analog + S/PDIF|S/PDIF only|analog only|analog only|"
echo "got: $got"; [ "$got" = "$want" ] && { echo PASS; exit 0; } || { echo "FAIL (want $want)"; exit 1; }
