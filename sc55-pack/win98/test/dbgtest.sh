#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# native test of SC55W.VXD's serial debug link (vxd/dbg98.c, w98.9); needs gcc-multilib (gcc -m32)
set -e
cd "$(dirname "$0")/.."
gcc -m32 -O1 -Wall -Wno-unknown-pragmas -Wno-unused-function -D__cdecl= -o /tmp/dbgtest test/dbgtest.c
/tmp/dbgtest
