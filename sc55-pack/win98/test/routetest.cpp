/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
// routetest.cpp - native unit tests for w98.8: MIDI output switching (midiroute.cpp: byte-exact Windows MIDI out,
// timing, tracker replay order / reset gap / silence) and the pad -> key filters (pad.cpp, DOS beta.125 rules).
// Build + run: sh test/routetest.sh   (Linux g++, Win32 shim in test/w32shim)
#include "../helper/midiroute.h"
#include "../helper/pad.h"
#include <cstdio>
#include <cstdlib>
#include <mmsystem.h>
#include <vector>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint32_t clk() { return GetTickCount() * 1000u; }
static void wait_out(size_t n) { for (int i = 0; i < 400 && g_fake_out.size() < n; i++) Sleep(5); }

static void test_bytes()
{
    // running status, a SysEx, real time inside a message, a status ending an open SysEx
    const uint8_t in[] = {0x90, 60, 100, 62, 90, 0xF8, 0xB0, 7, 100, 10, 64, 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00,
                          0x7F, 0x00, 0x41, 0xF7, 0xC1, 5, 0xF0, 0x7E, 0x7F, 0x80, 60, 0, 0xE0, 0, 64};
    const uint8_t want[] = {0x90, 60, 100, 0x90, 62, 90, 0xF8, 0xB0, 7, 100, 0xB0, 10, 64, 0xF0, 0x41, 0x10, 0x42, 0x12,
                            0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7, 0xC1, 5, 0xF0, 0x7E, 0x7F, 0xF7, 0x80, 60, 0, 0xE0, 0, 64};
    g_fake_out.clear(); g_fake_t.clear();
    MidiOutDev d; std::string err;
    CHECK(d.open(1, clk, err), "open %s", err.c_str());
    for (uint8_t b : in) d.put(b, 0);
    wait_out(sizeof want);
    CHECK(g_fake_out.size() == sizeof want && !memcmp(g_fake_out.data(), want, sizeof want), "bytes differ (%zu)", g_fake_out.size());
    d.close();
    printf("bytes: %zu out, %s\n", g_fake_out.size(), g_fake_out.size() == sizeof want ? "exact" : "WRONG");
}

static void test_timing()
{
    g_fake_out.clear(); g_fake_t.clear();
    MidiOutDev d; std::string err;
    d.open(1, clk, err);
    DWORD t0 = GetTickCount();
    for (int i = 0; i < 5; i++) { uint8_t m[3] = {0x90, (uint8_t)(60 + i), 100}; for (uint8_t b : m) d.put(b, (t0 + 50 + 40 * (uint32_t)i) * 1000u); }
    wait_out(15);
    Sleep(50);
    int worst = 0;
    for (int i = 0; i < 5; i++) {
        int dt = (int)(g_fake_t[(size_t)i * 3] - (t0 + 50 + 40 * (DWORD)i));
        if (abs(dt) > abs(worst)) worst = dt;
    }
    CHECK(worst >= -2 && worst <= 6, "byte times off by %d ms", worst);
    printf("timing: worst %d ms from the event's time\n", worst);
    d.close();
}

static void test_replay()
{
    MidiTracker tk;
    const uint8_t song[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7,     // GS reset
                            0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x11, 0x15, 0x02, 0x18, 0xF7,     // part 2 = drums
                            0xB0, 0, 8, 0xC0, 25, 0xB0, 7, 90, 11, 70, 91, 40, 101, 0, 100, 0, 6, 12,
                            0xE0, 0, 80, 0x91, 64, 100, 0xF0, 0x43};                                 // half a SysEx at the end
    for (uint8_t b : song) tk.byte(b);
    std::vector<int> r;                     // bytes, -n = a gap of n clocks
    tk.replay([&](uint8_t b) { r.push_back(b); }, [&](int n) { r.push_back(-n); });
    // the reset is followed by a gap of >= 200 clocks (64 ms), before anything else
    size_t i = 0;
    while (i < r.size() && r[i] != 0xF0) i++;
    CHECK(i + 11 < r.size() && r[i + 11] <= -200, "no reset gap");
    // channel 1: CC7 90 and CC11 70 come after program and bend
    int p_prog = -1, p_bend = -1, p_vol = -1, p_exp = -1;
    for (size_t k = 0; k + 2 < r.size(); k++) {
        if (r[k] == 0xC0 && r[k + 1] == 25) p_prog = (int)k;
        if (r[k] == 0xE0 && r[k + 1] == 0 && r[k + 2] == 80) p_bend = (int)k;
        if (r[k] == 0xB0 && r[k + 1] == 7 && r[k + 2] == 90) p_vol = (int)k;
        if (r[k] == 0xB0 && r[k + 1] == 11 && r[k + 2] == 70) p_exp = (int)k;
    }
    CHECK(p_prog > 0 && p_bend > p_prog && p_vol > p_bend && p_exp > p_vol, "order prog %d bend %d vol %d exp %d", p_prog, p_bend, p_vol, p_exp);
    CHECK(r.size() >= 2 && r[r.size() - 2] == 0xF0 && r.back() == 0x43, "half SysEx not resumed");
    std::vector<uint8_t> s;
    tk.silence([&](uint8_t b) { s.push_back(b); });
    CHECK(s.size() == 1 + 16 * 12 && s[0] == 0xF7, "silence %zu bytes", s.size());
    printf("replay: %zu items, reset gap ok, vol/exp last, silence %zu bytes\n", r.size(), s.size());
}

static void test_list()
{
    auto l = midi_out_list();
    CHECK(l.size() == 1 && l[0].id == 1, "list should skip NUKED SB-EMU");
}

// per poll: bytes sent (stick X, button A); the pad unplugs at the end (re-arm + release)
static std::vector<size_t> steps(const std::vector<int>& lx, const std::vector<int>& btn_a = {})
{
    std::vector<size_t> v;
    PadInput in; in.present = true;
    for (size_t i = 0; i < lx.size(); i++) {
        in.axis[PA_LX] = lx[i];
        in.button[PB_A] = i < btn_a.size() && btn_a[i];
        PadOutput o;
        pad_step(in, true, o);
        v.push_back(o.scancodes.size());
    }
    PadInput gone; PadOutput o; pad_step(gone, true, o);
    return v;
}
static size_t sum(const std::vector<size_t>& v) { size_t n = 0; for (size_t x : v) n += x; return n; }

static void test_pad()
{
    FILE* f = fopen("/tmp/routetest-pad.txt", "w");
    fputs("[Default]\nLStickX = Left Right\nA = Space\n", f);
    fclose(f);
    std::string log;
    pad_load("/tmp/none.txt", "/tmp/routetest-pad.txt", log);
    pad_set_deadzone(10);
    size_t drift = sum(steps({0, 14000, 15000, 14500, 15500, 14000, 15000, 0}));
    CHECK(drift == 0, "drift around 14000-15500 typed %zu bytes (threshold 16000)", drift);
    size_t rest_end = sum(steps({30000, 30000, 30000, 30000}));
    CHECK(rest_end == 0, "an axis resting at its end typed %zu bytes (must be seen centred first)", rest_end);
    size_t spike = sum(steps({0, 0, 25000, 0, 0}));
    CHECK(spike == 0, "one-poll spike typed %zu bytes", spike);
    auto h = steps({0, 20000, 20000, 13000, 13000, 11000, 0});
    CHECK(h[1] == 0 && h[2] > 0 && h[3] == 0 && h[4] == 0 && h[5] > 0, "push/hold/release %zu %zu %zu %zu %zu", h[1], h[2], h[3], h[4], h[5]);
    size_t glitch = sum(steps({0, 0, 0, 0, 0}, {0, 1, 0, 0, 0}));
    CHECK(glitch == 0, "one-poll button glitch typed %zu bytes", glitch);
    auto p = steps({0, 0, 0, 0, 0}, {0, 1, 1, 1, 0});
    CHECK(p[1] == 0 && p[2] > 0 && p[4] == 0, "button press %zu %zu %zu", p[1], p[2], p[4]);
    printf("pad: drift %zu, resting end %zu, spike %zu, glitch %zu bytes; push->key at poll 2, held at 13000, off at 11000\n",
           drift, rest_end, spike, glitch);
}

int main()
{
    test_bytes();
    test_timing();
    test_replay();
    test_list();
    test_pad();
    printf(fails ? "routetest: %d FAILED\n" : "routetest: PASS\n", fails);
    return fails ? 1 : 0;
}
