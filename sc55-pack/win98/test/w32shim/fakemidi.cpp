/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* fakemidi.cpp - the fake Windows MIDI out devices of the native tests (w32shim/mmsystem.h) */
#include <mmsystem.h>
#include <cstring>
std::vector<uint8_t> g_fake_out;
std::vector<DWORD> g_fake_t;
int g_fake_ndev = 2;
static pthread_mutex_t g_fm = PTHREAD_MUTEX_INITIALIZER;
static void rec(uint8_t b) { pthread_mutex_lock(&g_fm); g_fake_out.push_back(b); g_fake_t.push_back(GetTickCount()); pthread_mutex_unlock(&g_fm); }
MMRESULT midiOutOpen(HMIDIOUT* h, UINT, DWORD_PTR, DWORD_PTR, DWORD) { *h = (HMIDIOUT)1; return 0; }
MMRESULT midiOutClose(HMIDIOUT) { return 0; }
MMRESULT midiOutReset(HMIDIOUT) { return 0; }
MMRESULT midiOutShortMsg(HMIDIOUT, DWORD m)
{
    uint8_t s = (uint8_t)m;
    int n = s >= 0xF8 ? 1 : s >= 0xF0 ? (s == 0xF2 ? 3 : (s == 0xF1 || s == 0xF3) ? 2 : 1) : ((s & 0xE0) == 0xC0 ? 2 : 3);
    for (int i = 0; i < n; i++) rec((uint8_t)(m >> (8 * i)));
    return 0;
}
MMRESULT midiOutLongMsg(HMIDIOUT, MIDIHDR* m, UINT) { for (DWORD i = 0; i < m->dwBufferLength; i++) rec((uint8_t)m->lpData[i]); m->dwFlags |= MHDR_DONE; return 0; }
MMRESULT midiOutPrepareHeader(HMIDIOUT, MIDIHDR*, UINT) { return 0; }
MMRESULT midiOutUnprepareHeader(HMIDIOUT, MIDIHDR*, UINT) { return 0; }
MMRESULT midiOutGetErrorTextA(MMRESULT, char* t, UINT) { t[0] = 0; return 0; }
UINT midiOutGetNumDevs(void) { return (UINT)g_fake_ndev; }
MMRESULT midiOutGetDevCapsA(UINT id, MIDIOUTCAPSA* c, UINT)
{
    strcpy(c->szPname, id == 0 ? "NUKED SB-EMU" : id == 1 ? "USB Audio Device" : "SB16 MIDI Out");
    return 0;
}

