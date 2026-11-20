#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# native test of SC55W.VXD's MPU-401 front end (vxd/mpu98.c) against HMI SOS / Miles loops; needs gcc-multilib
set -e
cd "$(dirname "$0")/.."
gcc -m32 -O1 -Wall -Wno-unknown-pragmas -Wno-unused-function -o /tmp/mputest test/mputest.c
/tmp/mputest
