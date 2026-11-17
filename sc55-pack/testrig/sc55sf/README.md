# testrig/sc55sf (SC55SF task, wip)
- Reference renderer: `sudo apt-get install -y librtmidi-dev libsdl2-dev; cmake -S sc55-pack/upstream/Nuked-SC55 -B /tmp/nkb -DCMAKE_BUILD_TYPE=Release; cmake --build /tmp/nkb --target nuked-sc55-render -j2`
  then `/tmp/nkb/nuked-sc55-render -d ROMDIR -o out.wav song.mid` (avmidi songs: sc55-pack/upstream/Nuked-SC55/test/integration/avmidi).
- ROMs: priavte store-personal (tmp/p52/NUKEDSB/FILES/ROMS: M1_ROM1/ROM2/WAVE1-3.BIN = SC-55 mk1, ctrl Ver1.21).
- Parser reference: `git clone --depth 1 https://github.com/skjelten/emusc /tmp/dl/emusc` (libEmuSC, LGPL-2.1+).
- Layout + mapping plan: docs/SC55ROM.md.
