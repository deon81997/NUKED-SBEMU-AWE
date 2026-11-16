/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
// awe.h - AWE32 (EMU8000) for DOS boxes: the block shared with SC55W.VXD (sc55wif.h SC55W_AWE) and the
// DOS engine awe/synth/emu8k.c. The render thread applies the AWE ring by time and publishes the snapshot the
// VxD answers reads from; a helper thread drains the ring early if the render thread falls behind.
#pragma once
#include <string>
#include "../common/sc55wif.h"
#include "opts.h"

struct AweInfo {
    bool on = false;            // block ready and the engine running
    int port = 0, ram_kb = 0;
    std::string rom;            // ROM file found ("" = none: ROM sounds silent)
    std::string error;
};

// (re)builds the block for these options and fills sh->awe_*; call after DETACH, before ATTACH
const AweInfo& awe_prepare(const Options& o, const std::string& romdir, SC55W_SHARED* sh);
// after ATTACH: starts the ring watchdog thread
void awe_attached(unsigned long status);
void awe_shutdown();
const AweInfo& awe_info();

// render thread
SC55W_AWE* awe_block();          // NULL = AWE off
void awe_lock();
void awe_unlock();
void awe_apply(const SC55W_AWEEV& e);            // one ring entry into the engine (locked)
void awe_render(int16_t* out, int frames, int rate);
void awe_snapshot();
void awe_fx(int on);                             // reverb + chorus on/off (/AWEFX), applied when it changes

// ---- "AWE32 MIDI Synth": the second Windows MIDI device (SC55MIDI.DRV dev 1, EV_MIDI c = 2) plays the EMU8000
// with Creative's SBK banks (SYNTHGM/GS/MT.SBK, found by content under the pack and copied to ROMS) or NUKED's own GM
// map over the ROM. A DOS box that touches the EMU8000 ports gets the chip (Windows voices stop); the MIDI device
// takes it back when that DOS box is closed or has left the ports alone for 10 s. Settings: SC55W.INI [AWE32 MIDI].
struct AweMidiCfg {
    bool on = true;
    int bank = 0;              // 0 GM, 1 GS, 2 MT-32 (AWEUTIL /EM:GM|GS|MT32)
    int reverb = 4, chorus = 2; // Creative's AWE Control types (Hall 2, Chorus 3)
};
struct AweMidiInfo {
    std::string file[3];       // SYNTHGM / SYNTHGS / SYNTHMT in ROMS ("" = none)
    std::string using_;        // what plays now (bank names)
    bool builtin = false;      // NUKED's own GM map
    std::string note;          // scan / load messages
};
void awe_midi_load_cfg(const std::string& ini, AweMidiCfg& c);
void awe_midi_save_cfg(const std::string& ini, const AweMidiCfg& c);
// find Creative SBK banks by content under root (recursive) and copy them to romdir; returns a one-line summary
std::string awe_midi_scan(const std::string& root, const std::string& romdir);
// settings (any thread): loads the banks of romdir, applies bank / effects; re-attach needed if `on` changed
void awe_midi_config(const AweMidiCfg& c, const std::string& romdir);
const AweMidiCfg& awe_midi_cfg();
AweMidiInfo awe_midi_info();
// render thread
bool awe_midi_ready();                        // engine up and the device on
void awe_midi_byte(uint8_t b);                // locked by the caller
void awe_midi_poll(bool dos_wrote);           // DOS ownership; dos_wrote = an AWE ring write is applied now (locked)
bool awe_midi_windows();                      // Windows has the EMU8000 (its volume applies)
std::string awe_midi_stats();                 // SC55W.LOG line
