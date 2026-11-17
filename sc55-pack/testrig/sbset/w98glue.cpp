// SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sbset)
// Win98 Settings' SBOPTS.TXT reader/writer (win98/helper/opts.cpp) seen from C: what its AWE32 tab would show.
#include "../../win98/helper/opts.h"
#include <cstring>
extern "C" void w98_read(const char* line, char* out, int outsz, int* awe, int* ram, int* mpu, int* fx, char* rom, int romsz)
{
    Options o; o.parse(line);
    std::string f = o.format();
    snprintf(out, outsz, "%s", f.c_str());
    *awe = o.awe; *ram = o.aweram; *mpu = o.awempu; *fx = o.awefx;
    snprintf(rom, romsz, "%s", o.awerom.c_str());
}
