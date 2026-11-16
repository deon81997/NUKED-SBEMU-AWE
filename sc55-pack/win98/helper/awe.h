/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
// awe.h - AWE32 (EMU8000) for DOS boxes: the block shared with SC55W.VXD (sc55wif.h SC55W_AWE) and the
// DOS engine awe/synth/emu8k.c. Mirror mode: the render thread applies the AWE ring by time and publishes
// the snapshot; stall mode: a thread answers the VxD's reads at once (SC55W.INI [AWE] Mode=stall).
#pragma once
#include <string>
#include "../common/sc55wif.h"
#include "opts.h"

struct AweInfo {
    bool on = false;            // block ready and the engine running
    int port = 0, ram_kb = 0, mode = AWE_MIRROR;
    std::string rom;            // ROM file found ("" = none: ROM sounds silent)
    std::string error;
};

// (re)builds the block for these options and fills sh->awe_*; call after DETACH, before ATTACH
const AweInfo& awe_prepare(const Options& o, const std::string& romdir, SC55W_SHARED* sh, int mode);
// after ATTACH: starts the stall thread when the VxD took stall mode
void awe_attached(void* vxd, unsigned long status);
void awe_shutdown();
const AweInfo& awe_info();

// render thread
SC55W_AWE* awe_block();          // NULL = AWE off
void awe_lock();
void awe_unlock();
void awe_apply(const SC55W_AWEEV& e);            // one ring entry into the engine (locked)
void awe_render(int16_t* out, int frames, int rate);
void awe_snapshot();                             // dynamic registers for the VxD's mirror
bool awe_stall_mode();
