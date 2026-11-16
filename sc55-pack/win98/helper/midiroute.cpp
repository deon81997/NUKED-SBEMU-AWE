/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
// midiroute.cpp - see midiroute.h. Tracker / silence / replay follow sc55-pack/main.c (TK_*, MAIN_Silence,
// MAIN_Replay) of DOS beta.129 so both sides switch outputs the same way.
#include "midiroute.h"
#include <cstring>
#include <mmsystem.h>

// ---------------------------------------------------------------- tracker
void MidiTracker::clear()
{
    memset(cc_set, 0, sizeof cc_set); prog_set = pb_set = sens_set = 0; memset(rpn, 0x7F, sizeof rpn); rpn_init = true;
}

bool MidiTracker::is_reset(const uint8_t* m, int n)
{
    if (n >= 6 && m[1] == 0x7E && m[3] == 0x09 && (m[4] == 0x01 || m[4] == 0x03)) return true;                 // GM on
    if (n >= 11 && m[1] == 0x41 && m[3] == 0x42 && m[4] == 0x12 && m[5] == 0x40 && m[6] == 0x00 && m[7] == 0x7F) return true;   // GS reset
    if (n >= 9 && m[1] == 0x43 && m[3] == 0x4C && m[4] == 0 && m[5] == 0 && m[6] == 0x7E) return true;           // XG on
    if (n >= 8 && m[1] == 0x41 && m[3] == 0x16 && m[4] == 0x12 && m[5] == 0x7F) return true;                    // MT-32 reset
    return false;
}

// a Roland DT1 to an address already kept replaces that one; SC-55 display SysEx (model 45h) is not kept
void MidiTracker::sx_store(const uint8_t* m, int n)
{
    int l;
    if (n >= 5 && m[1] == 0x41 && m[3] == 0x45) return;
    if (n >= 10 && m[1] == 0x41 && m[4] == 0x12)
        for (int i = 0; i < sxn; i += l) {
            const uint8_t* k = sx + i;
            for (l = 1; i + l < sxn && k[l] != 0xF7; l++) {}
            l++;
            if (l == n && !memcmp(k, m, 8)) { memcpy(sx + i, m, (size_t)n); return; }
        }
    if (sxn + n <= SXMAX) { memcpy(sx + sxn, m, (size_t)n); sxn += n; }
}

void MidiTracker::msg(uint8_t st, const uint8_t* dd)
{
    int ch = st & 15;
    switch (st & 0xF0) {
    case 0xC0: prog[ch] = dd[0]; prog_set |= (uint16_t)(1 << ch); break;
    case 0xE0: pb[ch][0] = dd[0]; pb[ch][1] = dd[1]; pb_set |= (uint16_t)(1 << ch); break;
    case 0xB0:
        if (dd[0] == 121) {
            static const uint8_t r[] = { 1, 11, 64, 65, 66, 67 };
            for (int c = 0; c < 6; c++) cc_set[ch][r[c] >> 5] &= ~(1u << (r[c] & 31));
            pb_set &= (uint16_t)~(1 << ch);
        } else if (dd[0] == 101 || dd[0] == 100) rpn[ch][dd[0] == 100] = dd[1];
        else if (dd[0] == 6 || dd[0] == 38) { if (!rpn[ch][0] && !rpn[ch][1]) { sens[ch][dd[0] == 38] = dd[1]; sens_set |= (uint16_t)(1 << ch); } }
        else if (dd[0] < 120 && !(dd[0] >= 96 && dd[0] <= 99)) { cc[ch][dd[0]] = dd[1]; cc_set[ch][dd[0] >> 5] |= 1u << (dd[0] & 31); }
        break;
    }
}

void MidiTracker::byte(uint8_t b)
{
    if (!rpn_init) clear();
    if (b >= 0xF8) return;
    if (b == 0xF0) { insx = 1; cur[0] = b; curn = 1; rs = 0; have = 0; return; }
    if (insx) {
        if (b < 0x80) { if (curn < (int)sizeof cur) cur[curn] = b; curn++; return; }
        insx = 0;
        if (b == 0xF7 && curn < (int)sizeof cur) {
            cur[curn++] = b;
            if (is_reset(cur, curn)) { clear(); sxn = 0; }
            sx_store(cur, curn);
        }
        if (b == 0xF7) return;
    }
    if (b >= 0x80) {
        have = 0;
        if (b < 0xF0) { rs = b; need = (b & 0xE0) == 0xC0 ? 1 : 2; } else rs = 0;
        return;
    }
    if (!rs) return;
    d[have++] = b;
    if (have < need) return;
    have = 0;
    msg((uint8_t)rs, d);
}

static void put3(const std::function<void(uint8_t)>& o, int a, int b, int c)
{
    o((uint8_t)a); o((uint8_t)b); if (c >= 0) o((uint8_t)c);
}

// the old output: end a half-sent SysEx, then per channel sustain off, All Sound Off, All Notes Off, Reset All
void MidiTracker::silence(const std::function<void(uint8_t)>& o) const
{
    if (insx) o(0xF7);
    for (int ch = 0; ch < 16; ch++) { put3(o, 0xB0 | ch, 64, 0); put3(o, 0xB0 | ch, 120, 0); put3(o, 0xB0 | ch, 123, 0); put3(o, 0xB0 | ch, 121, 0); }
}

// the new output: every channel silent (sound off, expression 0), the reset + SysEx with time to take them, then
// per channel controllers, bank + program, bend range, bend, and volume + expression LAST; then running status
void MidiTracker::replay(const std::function<void(uint8_t)>& o, const std::function<void(int)>& gap) const
{
    int l;
    for (int ch = 0; ch < 16; ch++) { put3(o, 0xB0 | ch, 120, 0); put3(o, 0xB0 | ch, 11, 0); }
    for (int i = 0; i < sxn; i += l) {
        for (l = 0; i + l < sxn && sx[i + l] != 0xF7; l++) o(sx[i + l]);
        if (i + l < sxn) o(0xF7);
        l++;
        gap(is_reset(sx + i, l) ? 200 : 16);       // ~64 ms after a reset, ~5 ms after other SysEx
    }
    for (int ch = 0; ch < 16; ch++) {
        put3(o, 0xB0 | ch, 121, 0);
        put3(o, 0xB0 | ch, 11, 0);                 // 121 sets expression back to 127
        for (int c = 0; c < 120; c++) if (c != 7 && c != 11 && has_cc(ch, c)) put3(o, 0xB0 | ch, c, cc[ch][c]);
        if (prog_set & (1 << ch)) put3(o, 0xC0 | ch, prog[ch], -1);
        if (sens_set & (1 << ch)) {
            put3(o, 0xB0 | ch, 101, 0); put3(o, 0xB0 | ch, 100, 0); put3(o, 0xB0 | ch, 6, sens[ch][0]); put3(o, 0xB0 | ch, 38, sens[ch][1]);
            put3(o, 0xB0 | ch, 101, 127); put3(o, 0xB0 | ch, 100, 127);
        }
        if (rpn_init && (rpn[ch][0] != 0x7F || rpn[ch][1] != 0x7F)) { put3(o, 0xB0 | ch, 101, rpn[ch][0]); put3(o, 0xB0 | ch, 100, rpn[ch][1]); }
        if (pb_set & (1 << ch)) put3(o, 0xE0 | ch, pb[ch][0], pb[ch][1]);
        if (has_cc(ch, 7)) put3(o, 0xB0 | ch, 7, cc[ch][7]);
        put3(o, 0xB0 | ch, 11, has_cc(ch, 11) ? cc[ch][11] : 127);
    }
    if (insx) for (int i = 0; i < curn && i < (int)sizeof cur; i++) o(cur[i]);
    else if (rs) { o((uint8_t)rs); for (int i = 0; i < have; i++) o(d[i]); }
}

// ---------------------------------------------------------------- Windows MIDI out device
std::vector<MidiOutInfo> midi_out_list()
{
    std::vector<MidiOutInfo> v;
    UINT n = midiOutGetNumDevs();
    for (UINT i = 0; i < n; i++) {
        MIDIOUTCAPSA c;
        if (midiOutGetDevCapsA(i, &c, sizeof c) != MMSYSERR_NOERROR) continue;
        std::string s = c.szPname;
        if (s.find("NUKED") != std::string::npos) continue;    // our own driver: would loop back into SC55W
        v.push_back({i, s});
    }
    return v;
}

bool MidiOutDev::open(UINT id, Clock clk, std::string& err)
{
    close();
    MMRESULT r = midiOutOpen(&h, id, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR) {
        char t[MAXERRORLENGTH] = "";
        midiOutGetErrorTextA(r, t, sizeof t);
        err = t[0] ? t : "midiOutOpen failed";
        h = nullptr;
        return false;
    }
    clock = clk;
    head = tail = 0; quit = 0; mlen = mneed = 0; rs = 0; sysex.clear();
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD tid;
    th = CreateThread(NULL, 0, thread_main, this, 0, &tid);
    // above normal, below the sound thread: a sound card's MPU-401 driver busy-waits at cable speed in midiOutShortMsg,
    // so a burst (a replay, a backlog) must not starve the synth / Windows (86Box PII-450: 47 K bytes stalled SC55W)
    if (th) SetThreadPriority(th, THREAD_PRIORITY_ABOVE_NORMAL);
    return true;
}

void MidiOutDev::close()
{
    if (th) {
        InterlockedExchange(&quit, 1);
        SetEvent(ev);
        WaitForSingleObject(th, 3000);
        CloseHandle(th); th = nullptr;
    }
    if (ev) { CloseHandle(ev); ev = nullptr; }
    if (h) { midiOutReset(h); midiOutClose(h); h = nullptr; }
}

void MidiOutDev::push(uint32_t t, uint16_t v)
{
    LONG hd = head;
    if ((unsigned)(hd - tail) >= RING) { n_drop++; return; }
    ring[(unsigned)hd % RING] = {t, v};
    InterlockedExchange(&head, hd + 1);
    if (hd == tail) SetEvent(ev);              // the thread only waits when the ring is empty: one signal is enough
}
void MidiOutDev::put(uint8_t b, uint32_t t_us) { if (h) push(t_us, b); }
void MidiOutDev::pause(int ms) { if (h) push(0, (uint16_t)(0x100 + (ms > 1000 ? 1000 : ms))); }

DWORD WINAPI MidiOutDev::thread_main(LPVOID p) { ((MidiOutDev*)p)->run(); return 0; }

void MidiOutDev::send_long(const uint8_t* p, size_t n)
{
    MIDIHDR m;
    memset(&m, 0, sizeof m);
    m.lpData = (LPSTR)p; m.dwBufferLength = m.dwBytesRecorded = (DWORD)n;
    if (midiOutPrepareHeader(h, &m, sizeof m) != MMSYSERR_NOERROR) { n_drop += (unsigned)n; return; }
    if (midiOutLongMsg(h, &m, sizeof m) == MMSYSERR_NOERROR) {
        for (int i = 0; i < 3000 && !(m.dwFlags & MHDR_DONE); i++) Sleep(1);
        n_long++; n_sent += (unsigned)n;
    } else n_drop += (unsigned)n;
    midiOutUnprepareHeader(h, &m, sizeof m);
}

// bytes -> Windows messages: short messages with their status (running status resolved), SysEx as one long message
void MidiOutDev::out_byte(uint8_t b)
{
    if (b >= 0xF8) { midiOutShortMsg(h, b); n_sent++; return; }
    if (b == 0xF0) { sysex.assign(1, b); mlen = 0; rs = 0; return; }
    if (!sysex.empty()) {
        if (b < 0x80) { if (sysex.size() < 65536) sysex.push_back(b); return; }
        sysex.push_back(0xF7);                      // F7 or a status byte ends it
        send_long(sysex.data(), sysex.size());
        sysex.clear();
        if (b == 0xF7) return;
    }
    if (b == 0xF7) return;
    if (b >= 0x80) {
        rs = b < 0xF0 ? b : 0;
        msg[0] = b; mlen = 1;
        mneed = b >= 0xF0 ? (b == 0xF2 ? 2 : (b == 0xF1 || b == 0xF3) ? 1 : 0) : ((b & 0xE0) == 0xC0 ? 1 : 2);
        if (!mneed) { midiOutShortMsg(h, b); n_sent++; mlen = 0; }
        return;
    }
    if (!mlen) { if (!rs) return; msg[0] = rs; mlen = 1; mneed = (rs & 0xE0) == 0xC0 ? 1 : 2; }
    msg[mlen++] = b;
    if (mlen - 1 < mneed) return;
    DWORD w = msg[0] | (DWORD)msg[1] << 8 | (mlen > 2 ? (DWORD)msg[2] << 16 : 0);
    midiOutShortMsg(h, w); n_sent += (unsigned)mlen;
    mlen = 0;
}

void MidiOutDev::run()
{
    while (!quit) {
        if (head == tail) { WaitForSingleObject(ev, 10); continue; }
        Item it = ring[(unsigned)tail % RING];
        if (it.v >= 0x100) { Sleep(it.v - 0x100); InterlockedIncrement(&tail); continue; }
        if (it.t && clock) {                        // each byte at its time (the synth's latency, block jitter gone)
            int32_t w = (int32_t)(it.t - clock());
            if (w > 1500 && w < 2000000) { Sleep((DWORD)(w / 1000) > 1 ? (DWORD)(w / 1000) - 1 : 1); continue; }
        }
        out_byte((uint8_t)it.v);
        InterlockedIncrement(&tail);
    }
}
