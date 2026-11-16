#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# common/nsbver.h from win98/VERSION: every Win98 binary carries "$VER: NUKED SB-EMU <version> $" (as DOS packver.c)
cd "$(dirname "$0")/.."
v=$(cat VERSION)
printf '#define NSB_VER "%s"\n#define NSB_VER_TAG(name) "$VER: NUKED SB-EMU " NSB_VER " (" name ") $"\n' "$v" > common/nsbver.h.new
cmp -s common/nsbver.h.new common/nsbver.h && rm common/nsbver.h.new || mv common/nsbver.h.new common/nsbver.h
