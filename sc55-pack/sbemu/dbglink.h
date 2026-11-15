/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
#ifndef _DBGLINK_H_
#define _DBGLINK_H_
/* SBEMU /DBG:COM1|COM2[,baud][,level] - live debug link over a serial port (tools/dbg.h does the UART + ring).
 * Lines: "<seconds.ms> SB <kind> <text>", kind E error (sent before SBEMU goes on), I event, S per-second stats,
 * F per-frame summary (level 2), T trace record (level 3 capture), ! fatal. Tools' lines arrive as "<s.ms> MENU ...".
 * Level 1 = events + errors + stats once a second (default); 2 = + every port / DMA / IRQ summary per sound interrupt;
 * 3 = level 1 + every trapped port access into a RAM ring, sent only after a trigger (Ctrl+Alt+F10, SBDIAG DBG DUMP).
 * Off: every hook is one test of DBGL_On (0). */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern volatile int DBGL_On;           /* level, 0 = off */
int  DBGL_Init(const char* spec, char* msg, int size);  /* 0 ok (or spec empty/0/OFF = off), -1 bad spec, -2 no UART there: msg says */
void DBGL_Line(int sync, char kind, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void DBGL_Raw(int sync, const char* src, const char* text);   /* a tool's line (AMIS 0Ah ECX=1) */
void DBGL_Event(int trkind, const char* text);   /* trace.c: every flight-recorder event */
void DBGL_Isr(uint64_t isr_cycles);              /* end of SBEMU's sound interrupt */
void DBGL_Trap(uint32_t port, uint32_t val, int out);   /* every trapped port access (after the handler; val = in result) */
void DBGL_Midi(int dropped);
void DBGL_Under(uint32_t total);         /* render-ahead FIFO underruns so far */
void DBGL_Irq(void);
void DBGL_Ack(void);
void DBGL_Pump(void);                  /* idle / INT 28h: move bytes to the UART */
void DBGL_Trigger(void);               /* level 3: send the capture now */
void DBGL_HangTest(void);              /* forced hang at the next sound interrupt (test of "the last line gets out") */
void DBGL_Fatal(const char* why);      /* a "!" line, then wait until it has left the UART */
int  DBGL_Status(char* t, int size);   /* "DBG=COM1 BASE=3F8 BAUD=115200 LVL=1 FIFO=16 SENT=.. LOST=.." */
int  DBGL_SetLevel(int lvl);
#ifdef __cplusplus
}
#endif
#endif
