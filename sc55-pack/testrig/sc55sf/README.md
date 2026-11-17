# testrig/sc55sf (SC55SF task, wip)
- Reference renderer: `sudo apt-get install -y librtmidi-dev libsdl2-dev; cmake -S sc55-pack/upstream/Nuked-SC55 -B /tmp/nkb -DCMAKE_BUILD_TYPE=Release; cmake --build /tmp/nkb --target nuked-sc55-render -j2`
  then `/tmp/nkb/nuked-sc55-render -d ROMDIR -o out.wav song.mid` (avmidi songs: sc55-pack/upstream/Nuked-SC55/test/integration/avmidi).
- ROMs: priavte store-personal (tmp/p52/NUKEDSB/FILES/ROMS: M1_ROM1/ROM2/WAVE1-3.BIN = SC-55 mk1, ctrl Ver1.21).
- Parser reference: `git clone --depth 1 https://github.com/skjelten/emusc /tmp/dl/emusc` (libEmuSC, LGPL-2.1+).
- Layout + mapping plan: docs/SC55ROM.md.
- Tool: `gcc -O2 -o /tmp/sf sc55-pack/tools/sc55sf.c -lm; /tmp/sf ROMDIR /tmp/SC55.SF2 -v` (~2 s; -t/-r envelope/release time %, -f try the TVF base cutoff, -g gain dB).
- Measure: `python3 probe.py mid /tmp/r/probe.mid` (128 programs C4 + drum notes 35-81, dry), render it with nuked-sc55-render and
  `fluidsynth -ni -R 0 -C 0 -g 1.0 -r 44100 -F f.wav SC55.SF2 probe.mid`, then `python3 probe.py cmp n.wav f.wav 0 10`
  (per-item envelope dB, octave-band dB, pitch cents, worst list). Songs: `../awe/chromadtw.py n01.wav f01.wav`.
- 2026-11-16 (default -t550 -r350, no TVF): env 4.1 dB, band 3.1 dB, pitch median 2 c over 175 items; avmidi 01 chroma DTW 0.690.
