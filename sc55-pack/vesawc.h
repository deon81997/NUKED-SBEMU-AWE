/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* vesawc.h - SBEMU /VWC: optional write-combining (WC) for the VESA linear frame buffer (see vesawc.c). */
#ifndef VESAWC_H
#define VESAWC_H
#ifdef __cplusplus
extern "C" {
#endif

/* SBEMU start-up (first run and a run with SBEMU already resident). value = /VWC (0/1), given = /VWC was on the
 * command line, log = SBDETAIL.TXT printf (may be NULL). First run: /VWC:1 sets WC, kept while SBEMU stays resident,
 * undone if this SBEMU exits without going resident. Re-run: /VWC:1 sets it, /VWC:0 takes SBEMU's WC MTRR away. */
void VESAWC_Init(int value, int given, void (*log)(const char* fmt, ...));

/* one line for SBDIAG: "on (...)", "off (...)" or "unavailable - reason"; returns 1 on, 0 off, -1 unavailable */
int VESAWC_Status(char* out, int size);

#ifdef __cplusplus
}
#endif
#endif
