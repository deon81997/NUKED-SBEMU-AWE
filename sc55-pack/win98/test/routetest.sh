#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED
# native unit test of midiroute.cpp + pad.cpp (w98.8); needs g++ only
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -Wall -Wno-unused-function -Itest/w32shim -D_stricmp=strcasecmp -o /tmp/routetest test/routetest.cpp test/w32shim/fakemidi.cpp helper/midiroute.cpp helper/pad.cpp -lpthread
/tmp/routetest
