#!/bin/bash
# mkoracle.sh: build ./oracle = our Nuked-SC55 fork with trace hooks on control-ROM (rom2) data reads and PCM-chip
# register writes (patched copies in /tmp/sc55sf_core; the pack's sources stay untouched). GPL-2.0-or-later.
set -e
S=$(cd "$(dirname "$0")/../.." && pwd); B=/tmp/sc55sf_core; D=$(cd "$(dirname "$0")" && pwd)
rm -rf $B; cp -r $S/upstream/Nuked-SC55/src/backend $B
cat > $B/sfhook.h <<'H'
#pragma once
#include <stdint.h>
extern void (*sc55sf_pcmw)(int kind, int slot, int ix, uint32_t val);
extern uint8_t sc55sf_r2(const uint8_t* rom, uint32_t a);
extern void* sc55sf_pcm;
H
sed -i 's|pcm.ram1\[pcm.select_channel\]\[ix\] = pcm.write_latch;|&\n            if (sc55sf_pcmw) sc55sf_pcmw(1, pcm.select_channel, ix, pcm.write_latch);|' $B/pcm.cpp
sed -i 's|pcm.ram2\[pcm.select_channel\]\[ix\] = static_cast<uint16_t>(pcm.write_latch);|&\n            if (sc55sf_pcmw) sc55sf_pcmw(2, pcm.select_channel, ix, pcm.write_latch \& 0xffff);|' $B/pcm.cpp
sed -i 's|^    address \&= 0x3f;$|&\n    sc55sf_pcm = \&pcm;|' $B/pcm.cpp
sed -i 's|pcm.voice_mask_updating = true;|&\n        if (sc55sf_pcmw) sc55sf_pcmw(0, address \& 3, 0, data);|' $B/pcm.cpp
sed -i 's|ret = mcu.rom2\[address_rom & mcu.rom2_mask\];|ret = sc55sf_r2(mcu.rom2, address_rom \& mcu.rom2_mask);|' $B/mcu.cpp
grep -c sc55sf_ $B/pcm.cpp $B/mcu.cpp
FL="-std=gnu++20 -O2 -march=native -fno-rtti -DNDEBUG -I$S/sc55/config -I$S/sc55 -I$B -include $S/sc55/shim/sc55_compat.h -include $B/sfhook.h"
g++ $FL -o $D/oracle $D/oracle.cpp $S/sc55/sc55synth.cpp $B/*.cpp -x c $B/sha/sha224-256.c
