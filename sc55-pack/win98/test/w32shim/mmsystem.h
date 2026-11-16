/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* w32shim/mmsystem.h - fake midiOut device for routetest.cpp: records every byte it is given */
#pragma once
#include "windows.h"
#include <vector>
typedef void* HMIDIOUT;
#define MMSYSERR_NOERROR 0
#define MAXERRORLENGTH 128
#define CALLBACK_NULL 0
#define MHDR_DONE 1
typedef struct { char szPname[32]; } MIDIOUTCAPSA;
typedef struct { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded, dwFlags; } MIDIHDR;
extern std::vector<uint8_t> g_fake_out;   // bytes as the device got them (short messages unpacked, SysEx whole)
extern std::vector<DWORD> g_fake_t;        // GetTickCount() for each byte
extern int g_fake_ndev;
MMRESULT midiOutOpen(HMIDIOUT* h, UINT id, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT midiOutClose(HMIDIOUT h);
MMRESULT midiOutReset(HMIDIOUT h);
MMRESULT midiOutShortMsg(HMIDIOUT h, DWORD m);
MMRESULT midiOutLongMsg(HMIDIOUT h, MIDIHDR* m, UINT);
MMRESULT midiOutPrepareHeader(HMIDIOUT h, MIDIHDR* m, UINT);
MMRESULT midiOutUnprepareHeader(HMIDIOUT h, MIDIHDR* m, UINT);
MMRESULT midiOutGetErrorTextA(MMRESULT, char* t, UINT n);
UINT midiOutGetNumDevs(void);
MMRESULT midiOutGetDevCapsA(UINT id, MIDIOUTCAPSA* c, UINT);
