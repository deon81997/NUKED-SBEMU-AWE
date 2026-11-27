#!/bin/sh
# build.sh MUNT_SRC_DIR OUT_EXE [extra CXXFLAGS]  - host build of render.cpp against one Munt copy
# (same lib file list + defines as the DOS makefile's MT32_SRC/MT32_CXXFLAGS; -m32 -mfpmath=387 like DJGPP)
set -e
S=$1; O=$2; shift 2
D=$(dirname "$0"); B=/tmp/mt32host-$(basename "$O"); rm -rf "$B"; mkdir -p "$B"
for f in Analog BReverbModel Display File FileStream LA32FloatWaveGenerator LA32Ramp LA32WaveGenerator \
	MidiStreamParser Part Partial PartialManager Poly ROMInfo Synth Tables TVA TVF TVP sha1/sha1; do
	g++ -m32 -march=i686 -mfpmath=387 -std=gnu++11 -O2 -DNDEBUG -I"$S" "$@" -c "$S/$f.cpp" -o "$B/$(basename $f).o" &
done; wait
g++ -m32 -march=i686 -mfpmath=387 -std=gnu++11 -O2 -I"$S" "$@" "$D/render.cpp" "$B"/*.o -o "$O"
