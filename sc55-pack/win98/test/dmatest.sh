#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# native test of SC55W.VXD's DMA address proof (vxd/dma98.c, awe.7); needs gcc-multilib
set -e
cd "$(dirname "$0")/.."
gcc -m32 -O1 -Wall -Wextra -o /tmp/dmatest test/dmatest.c && /tmp/dmatest
