#!/bin/sh
# awempu_host.sh AWE32.RAW - host test of /AWEMPU:GM|GS|MT32 (sbemu/vawe.c + awe/synth) with stub qemm.h / vdiag.h,
# stand-in SBKs from TimGM6mb (sf2sbk.py, neutral file names: found by content). Also built with ASan/UBSan.
# Needs gcc, python3, timgm6mb-soundfont. Last line PASS or FAIL.
H=$(cd "$(dirname "$0")" && pwd); P=$(cd "$H/../.." && pwd); W=${W:-/tmp/awempu-host}
rm -rf "$W"; mkdir -p "$W/v/sbemu" "$W/roms" "$W/empty"
cp "$P/sbemu/vawe.c" "$P/sbemu/vawe.h" "$W/v/sbemu/"; ln -s "$P/awe" "$W/v/awe"
printf '#include <stdint.h>\n#define QEMM_IODT_SPLIT 0x40000000\ntypedef struct { uint32_t port; uint32_t (*handler)(uint32_t, uint32_t, uint32_t); } QEMM_IODT;\n' > "$W/v/qemm.h"
printf '#include <stdint.h>\nstatic inline int DIAG_ResetFlush(uint8_t c) { (void)c; return 0; }\nstatic inline int DIAG_Command(uint8_t c) { (void)c; return 0; }\nstatic inline int DIAG_ReadByte(uint8_t* b) { (void)b; return 0; }\nstatic inline void DIAG_Midi(int d) { (void)d; }\n' > "$W/v/sbemu/vdiag.h"
cp "$1" "$W/roms/AWE32.RAW"
python3 "$H/sf2sbk.py" /usr/share/sounds/sf2/TimGM6mb.sf2 "$W/roms/b_gm.dat" gm > /dev/null || exit 1
python3 "$H/sf2sbk.py" /usr/share/sounds/sf2/TimGM6mb.sf2 "$W/roms/b_gs.dat" gs > /dev/null || exit 1
python3 "$H/sf2sbk.py" /usr/share/sounds/sf2/TimGM6mb.sf2 "$W/roms/b_mt.dat" mt32 > /dev/null || exit 1
S="$W/v/sbemu/vawe.c $P/awe/synth/awemidi.c $P/awe/synth/emu8k.c $P/awe/synth/emu8k_rom.c"
cc -std=gnu99 -O2 -Wall -Wno-unused-function -Wno-format-truncation -I"$W/v" -I"$W/v/sbemu" "$H/awempu_host.c" $S -lm -o "$W/t" || exit 1
cc -std=gnu99 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -I"$W/v" -I"$W/v/sbemu" "$H/awempu_host.c" $S -lm -o "$W/ta" || exit 1

cd "$W" && "$W/t" roms empty | tee "$W/out.txt" | tail -n 16
"$W/ta" roms empty > "$W/asan.txt" 2>&1 && tail -n 1 "$W/asan.txt" | grep -q PASS || { echo "FAIL asan"; tail -n 5 "$W/asan.txt"; exit 1; }
tail -n 1 "$W/out.txt" | grep -q PASS && echo PASS || echo FAIL
