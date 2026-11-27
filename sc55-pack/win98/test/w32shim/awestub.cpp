/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
// awestub.cpp - native enginetest only: AWE32 off (awe_block() = NULL), so engine.cpp links without helper/awe.cpp
// (Windows file/critical-section code). The AWE engine itself is tested by the DOS/host AWE rigs.
#include <cstdint>
#include "../../helper/awe.h"
SC55W_AWE* awe_block() { return 0; }
void awe_lock() {} void awe_unlock() {}
void awe_apply(const SC55W_AWEEV&) {} void awe_render(int16_t*, int, int) {} void awe_snapshot() {} void awe_fx(int) {}
bool awe_midi_ready() { return false; } void awe_midi_byte(uint8_t) {} void awe_midi_poll(bool) {}
bool awe_midi_windows() { return false; } void awe_midi_dos_byte(uint8_t, int) {}
