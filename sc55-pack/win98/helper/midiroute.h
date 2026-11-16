/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
// midiroute.h - where the MIDI music goes: the emulated synth or a Windows MIDI output device (e.g. a USB MIDI
// cable to a real SC-55), switchable while a game plays without a restart (DOS: SBEMU /MOUT, beta.123 + 129).
//  - MidiTracker: the state a new output needs (last reset + SysEx, per channel controllers, program, RPN, bend,
//    running status / half message), the same rules as SBEMU's main.c TK_*;
//  - silence() for the old output and replay() for the new one, in the beta.129 order (no loud blast);
//  - MidiOutDev: a Windows midiOut device fed byte for byte from a thread, each byte at its own time.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

class MidiTracker {
public:
    void byte(uint8_t b);
    // o(byte); gap(n) = n timing-clock bytes of cable time (0.32 ms each) the new output needs to take a reset
    void silence(const std::function<void(uint8_t)>& o) const;
    void replay(const std::function<void(uint8_t)>& o, const std::function<void(int)>& gap) const;
private:
    static const int SXMAX = 2048;
    uint8_t prog[16] = {}, cc[16][128] = {}, pb[16][2] = {}, sens[16][2] = {}, rpn[16][2];
    uint16_t prog_set = 0, pb_set = 0, sens_set = 0;
    uint32_t cc_set[16][4] = {};
    uint8_t sx[SXMAX] = {}, cur[512] = {};
    int sxn = 0, curn = 0, insx = 0, rs = 0, have = 0, need = 0;
    uint8_t d[2] = {};
    bool rpn_init = false;
    void clear();
    void sx_store(const uint8_t* m, int n);
    void msg(uint8_t st, const uint8_t* dd);
    bool has_cc(int ch, int c) const { return (cc_set[ch][c >> 5] >> (c & 31)) & 1; }
public:
    static bool is_reset(const uint8_t* m, int n);
};

// one Windows MIDI output device, fed from a ring by its own thread
class MidiOutDev {
public:
    typedef uint32_t (*Clock)();        // the VxD clock in microseconds (events' time base)
    ~MidiOutDev() { close(); }
    bool open(UINT id, Clock clk, std::string& err);
    void close();
    bool is_open() const { return h != nullptr; }
    // a byte to send at time t_us (VxD clock; 0 = now); pause_ms = a gap the device needs (after a reset)
    void put(uint8_t b, uint32_t t_us);
    void pause(int ms);
    unsigned sent() const { return n_sent; }
    unsigned dropped() const { return n_drop; }
    unsigned long_msgs() const { return n_long; }
private:
    HMIDIOUT h = nullptr;
    HANDLE th = nullptr, ev = nullptr;
    volatile LONG quit = 0;
    Clock clock = nullptr;
    static const unsigned RING = 65536;
    struct Item { uint32_t t; uint16_t v; };    // v < 256 byte, 0x100 + ms = pause
    Item ring[RING];
    volatile LONG head = 0, tail = 0;
    unsigned n_sent = 0, n_drop = 0, n_long = 0;
    // thread side
    std::vector<uint8_t> sysex;
    uint8_t msg[3] = {}; int mlen = 0, mneed = 0; uint8_t rs = 0;
    void push(uint32_t t, uint16_t v);
    static DWORD WINAPI thread_main(LPVOID p);
    void run();
    void out_byte(uint8_t b);
    void send_long(const uint8_t* p, size_t n);
};

// the Windows MIDI output devices SC55W can send to (not its own NUKED SB-EMU driver)
struct MidiOutInfo { UINT id; std::string name; };
std::vector<MidiOutInfo> midi_out_list();
