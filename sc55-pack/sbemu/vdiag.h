/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
#ifndef _VDIAG_H_
#define _VDIAG_H_
/* SBEMU self-diagnostics for SBDIAG.EXE: measures where the CPU time goes while a game runs.
 * Off (one flag test per hook) until SBDIAG arms it with MPU command 0xA7; 0xA6 takes a snapshot
 * that the MPU data port then returns byte by byte. Nothing is printed or written by SBEMU itself. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

extern volatile int DIAG_Armed;

void DIAG_Calibrate(void);                 /* once at startup */
void DIAG_IsrBegin(void);                  /* sound card interrupt (ours) */
void DIAG_IsrEnd(int frames);
void DIAG_VirqBegin(void);                 /* game's Sound Blaster handler, run inside our interrupt */
void DIAG_VirqEnd(void);
void DIAG_Render(uint64_t cycles, int frames, int freq, int synth); /* synth: 0 SC-55, 1 SoundFont, 2 MT-32 */
void DIAG_RenderAwe(uint64_t cycles, int frames, int freq);                    /* AWE32 EMU8000, counted apart (layout 6) */
void DIAG_Trap(uint32_t port);             /* every trapped port access */
void DIAG_Midi(int dropped);               /* every MIDI byte the game sends */
void DIAG_Fifo(int fill_frames, uint32_t underrun_frames_total); /* render-ahead FIFO state after each interrupt */
void DIAG_SetSynthInfo(int gov_level, int fx_on, int voices);
void DIAG_SetHotkey(void (*fn)(void));       /* beta.123: Ctrl+Alt+F11 calls fn (from the sound interrupt) */
void DIAG_Toast(const char* msg, int beeps); /* a 2 s note top left (text / 13h) + 1-3 beeps */
uint64_t DIAG_Tsc(void);
uint64_t DIAG_CyclesPerSec(void);
void DIAG_TimerReplayed(uint32_t n);
void DIAG_BiosClock(uint64_t expected_milli, uint64_t got_milli); /* DOS clock vs real time while the game runs */ /* game timer ticks given back by SBEMU */

/* MPU-401 side channel: returns 1 if the command was a diagnostics command */
int DIAG_Command(uint8_t cmd);
/* returns 1 and the next byte while a snapshot is being read */
int DIAG_ResetFlush(uint8_t cmd);
int DIAG_ReadByte(uint8_t* b);

#ifdef __cplusplus
}
#endif
#endif
