/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* dbglink.c - SBEMU /DBG serial debug link: see dbglink.h. Everything here may run inside the sound interrupt or a port
 * trap: no DOS/BIOS calls, no allocation, polled UART writes of at most one FIFO load per call (dbg_pump), except
 * E / ! lines which wait until they have left (dbg_sync, time-limited). */
#define DBG_NO_TOOL
#include "../tools/dbg.h"
#include "dbglink.h"
#include "trace.h"
#include "vdiag.h"
#include "sbemu.h"
#include "vdma.h"
#include "usbpad.h"
#include "dpmi/dpmi.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <go32.h>
#include <sys/farptr.h>

volatile int DBGL_On;
static DbgPort P;
static int  L_Com;
static uint32_t L_Tick0;
static uint64_t L_T0, L_Cps, L_StatT, L_IsrCyc, L_IsrMax, L_LastIsr, L_IGap;
static uint32_t L_Isrs, L_Traps, L_Midi, L_Drop, L_Virq, L_Ack, L_Under, L_UnderLast, L_UnderInit, L_LostShown;
static uint32_t F_Cls[6], F_Virq, F_Ack;           /* level 2: this frame */
static char L_LastEv[64]; static uint32_t L_EvRep; static uint64_t L_EvT;
static volatile int L_Hang, L_Trig;
/* level 3 capture: every trapped port access */
#define CAP_N 4096u
typedef struct { uint32_t t; uint16_t port; uint8_t val, out; } CapRec;
static CapRec Cap[CAP_N];
static uint32_t Cap_W, Cap_R, Cap_End, Cap_T0;
static volatile int Cap_Dumping;

extern uint32_t TRACE_SdaLin(void);
/* the program running now, from DOS's current PSP and its memory block name. Read with _farpeek on _dos_ds:
 * DPMI_LoadB (LOAD_DS) from inside the sound interrupt froze 86Box's PII-450 + ES1371 rig (beta.127 test) */
static void prog_name(char* out)
{
    uint32_t sda = TRACE_SdaLin(), m; uint16_t psp; int i;
    strcpy(out, "?");
    if (!sda) return;
    psp = _farpeekw(_dos_ds, sda + 0x10);
    if (psp < 0x40 || psp > 0xA000) return;
    m = ((uint32_t)psp - 1) * 16;
    if ((_farpeekb(_dos_ds, m) == 'M' || _farpeekb(_dos_ds, m) == 'Z') && _farpeekw(_dos_ds, m + 1) == psp)
    {
        for (i = 0; i < 8; i++) { char c = (char)_farpeekb(_dos_ds, m + 8 + (uint32_t)i); if ((unsigned char)c < 33 || (unsigned char)c > 126) break; out[i] = c; }
        out[i] = 0;
        if (i) return;
    }
    sprintf(out, "PSP%04X", psp);
}

static uint64_t now_ms(void)
{
    uint64_t t;
    if (!L_Cps) L_Cps = DIAG_CyclesPerSec();
    if (!L_Cps) return (uint64_t)(_farpeekl(_dos_ds, 0x46C) - L_Tick0) * 55;
    t = DIAG_Tsc() - L_T0;
    return t / (L_Cps / 1000 ? L_Cps / 1000 : 1);
}
static int stamp(char* b)
{
    uint64_t ms = now_ms();
    return sprintf(b, "%lu.%03lu ", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
}
static void put_line(int sync, const char* src, char kind, const char* text)
{
    char b[240]; int n = stamp(b);
    if (kind) n += snprintf(b + n, sizeof b - 4 - (size_t)n, "%s %c %s", src, kind, text);
    else n += snprintf(b + n, sizeof b - 4 - (size_t)n, "%s %s", src, text);
    if (n > (int)sizeof b - 4) n = sizeof b - 4;
    while (n && (b[n - 1] == '\n' || b[n - 1] == '\r')) n--;
    b[n++] = '\r'; b[n++] = '\n';
    dbg_put(&P, b, n);
    if (sync) dbg_sync(&P); else dbg_pump(&P);
}
void DBGL_Line(int sync, char kind, const char* fmt, ...)
{
    char t[200]; va_list ap;
    if (!DBGL_On) return;
    va_start(ap, fmt); vsnprintf(t, sizeof t, fmt, ap); va_end(ap);
    put_line(sync, "SB", kind, t);
}
void DBGL_Raw(int sync, const char* src, const char* text) { if (DBGL_On) put_line(sync, src, 0, text); }

static void ev_flush_rep(void)
{
    if (L_EvRep) { DBGL_Line(0, 'I', "(last event %lu more times)", (unsigned long)L_EvRep); L_EvRep = 0; }
}
void DBGL_Event(int k, const char* text)
{
    uint64_t t;
    if (!DBGL_On) return;
    t = now_ms();
    if (!strncmp(L_LastEv, text, sizeof L_LastEv - 1) && t - L_EvT < 1000) { L_EvRep++; return; }   /* floods: counted */
    ev_flush_rep();
    strncpy(L_LastEv, text, sizeof L_LastEv - 1); L_EvT = t;
    /* errors go out before SBEMU goes on (a freeze right after still shows them) */
    DBGL_Line(k == TR_IRQ || k == TR_DMA, (k == TR_IRQ || k == TR_DMA) ? 'E' : 'I', "%s%s", k == TR_USB ? "usb " : k == TR_CD ? "cd " : "", text);
}
void DBGL_Midi(int dropped) { L_Midi++; if (dropped) L_Drop++; }
void DBGL_Irq(void) { L_Virq++; F_Virq++; }
void DBGL_Ack(void) { L_Ack++; F_Ack++; }
void DBGL_Under(uint32_t total)
{
    if (!L_UnderInit) { L_UnderLast = total; L_UnderInit = 1; }
    L_Under = total - L_UnderLast;
}

static int cls(uint32_t port)
{
    if (port >= 0x330 && port <= 0x331) return 0;
    if (port >= 0x220 && port <= 0x28F) return 1;
    if (port >= 0x388 && port <= 0x38B) return 2;
    if (port == 0x40 || port == 0x42 || port == 0x43 || port == 0x61) return 3;
    if (port == 0x20 || port == 0x21 || port == 0xA0 || port == 0xA1) return 4;
    return 5;
}
void DBGL_Trap(uint32_t port, uint32_t val, int out)
{
    L_Traps++;
    if (DBGL_On == 2) F_Cls[cls(port)]++;
    else if (DBGL_On == 3 && !Cap_Dumping)
    {
        CapRec* r = &Cap[Cap_W++ & (CAP_N - 1)];
        r->t = (uint32_t)DIAG_Tsc(); r->port = (uint16_t)port; r->val = (uint8_t)val; r->out = (uint8_t)(out ? 1 : 0);
    }
    if (P.head != P.tail) dbg_pump(&P);
}

/* level 3: format captured records while the ring has room (a few per call) */
static void cap_send(void)
{
    char b[48]; int n, k = 0; uint32_t us_div = (uint32_t)(L_Cps / 1000000u); CapRec* r;
    if (!us_div) us_div = 1;
    while (Cap_R != Cap_End && k++ < 24 && DBG_RING - (P.head - P.tail) > 64)
    {
        r = &Cap[Cap_R++ & (CAP_N - 1)];
        n = sprintf(b, "T %lu %s %03X %02X\r\n", (unsigned long)((r->t - Cap_T0) / us_div), r->out ? "o" : "i", r->port, r->val);
        dbg_put(&P, b, n);
    }
    if (Cap_R == Cap_End)
    {
        DBGL_Line(0, 'I', "capture end");
        Cap_W = 0; Cap_Dumping = 0;           /* capturing again */
    }
}
void DBGL_Trigger(void) { if (DBGL_On == 3) L_Trig = 1; }
static void cap_start(void)
{
    uint32_t n = Cap_W < CAP_N ? Cap_W : CAP_N;
    Cap_Dumping = 1;
    Cap_End = Cap_W; Cap_R = Cap_W - n; Cap_T0 = n ? Cap[Cap_R & (CAP_N - 1)].t : 0;
    DBGL_Line(0, 'I', "capture: last %lu port accesses (T us-since-first i/o port value)", (unsigned long)n);
}

void DBGL_HangTest(void) { L_Hang = 1; }
void DBGL_Fatal(const char* why)
{
    if (!DBGL_On) return;
    ev_flush_rep();
    put_line(1, "SB", '!', why);
}

void DBGL_Isr(uint64_t cyc)
{
    uint64_t t;
    if (!DBGL_On) return;
    L_Isrs++; L_IsrCyc += cyc; if (cyc > L_IsrMax) L_IsrMax = cyc;
    if (L_Hang)
    {   /* the test: tell, then stop like a hard freeze (interrupts off, nothing runs again) */
        L_Hang = 0;
        DBGL_Fatal("forced hang test (SBDIAG DBG HANG): interrupts off, this PC stops now");
        __asm__ volatile("cli");             /* under HDPMI a ring-3 CLI/HLT is virtualised: spin, never return */
        for (;;) __asm__ volatile("" ::: "memory");
    }
    if (DBGL_On == 2 && (F_Cls[0] | F_Cls[1] | F_Cls[2] | F_Cls[3] | F_Cls[4] | F_Cls[5] | F_Virq | F_Ack))
    {
        char b[160]; int n;
        n = sprintf(b, "F mpu=%lu sb=%lu opl=%lu pit=%lu pic=%lu other=%lu virq=%lu ack=%lu", (unsigned long)F_Cls[0],
            (unsigned long)F_Cls[1], (unsigned long)F_Cls[2], (unsigned long)F_Cls[3], (unsigned long)F_Cls[4], (unsigned long)F_Cls[5],
            (unsigned long)F_Virq, (unsigned long)F_Ack);
        if (SBEMU_HasStarted())
        {
            int dma = SBEMU_GetBits() <= 8 ? SBEMU_GetDMA() : SBEMU_GetHDMA();
            n += sprintf(b + n, " dma%d=%05lX+%lX/%lX", dma, (unsigned long)VDMA_GetAddress(dma), (unsigned long)VDMA_GetIndex(dma),
                (unsigned long)VDMA_GetCounter(dma));
        }
        memset(F_Cls, 0, sizeof F_Cls); F_Virq = F_Ack = 0;
        DBGL_Line(0, 0, "%s", b);
    }
    if (L_Trig && !Cap_Dumping) { L_Trig = 0; cap_start(); }
    if (Cap_Dumping) cap_send();
    if (!L_Cps) L_Cps = DIAG_CyclesPerSec();
    t = DIAG_Tsc();
    if (L_LastIsr && t - L_LastIsr > L_IGap) L_IGap = t - L_LastIsr;   /* beta.128: longest time without a sound interrupt */
    L_LastIsr = t;
    if (L_Cps && t - L_StatT >= L_Cps)
    {
        char pn[9]; uint64_t el = t - L_StatT; unsigned load = el ? (unsigned)(L_IsrCyc * 1000u / el) : 0;
        if (L_StatT)
        {
            ev_flush_rep();
            prog_name(pn);
            DBGL_Line(0, 'S', "prog=%s load=%u.%u%% isr=%lu max=%luus virq=%lu ack=%lu traps=%lu midi=%lu drop=%lu under=%lu sb=%s%lu/%d/%d lost=%lu igap=%luus",
                pn, load / 10, load % 10, (unsigned long)L_Isrs, (unsigned long)(L_IsrMax * 1000000u / L_Cps), (unsigned long)L_Virq,
                (unsigned long)L_Ack, (unsigned long)L_Traps, (unsigned long)L_Midi, (unsigned long)L_Drop, (unsigned long)L_Under,
                SBEMU_HasStarted() ? "" : "idle:", (unsigned long)SBEMU_GetSampleRate(), SBEMU_GetBits(), SBEMU_GetChannels(),
                (unsigned long)(P.lost - L_LostShown), (unsigned long)(L_IGap * 1000000u / L_Cps));
            L_LostShown = P.lost;
            {   /* beta.128: USB MIDI OUT (/MUSB): queue now/max, latency avg/max, longest wait to a transfer, transfers,
                 * packets, pacing holds, polls that found the transfer still busy (NAK), full USB poll time avg/max */
                USBPAD_MidiTimeT u;
                USBPAD_MidiTiming(&u, 1);
                if (u.tx || u.qmax || u.nak || u.retry)
                    DBGL_Line(0, 'S', "usbmidi tx=%lu bytes=%lu bmax=%lu q=%lu/%lu lat=%lu/%luus wait=%luus gap=%luus paced=%lu nak=%lu retry=%lu poll=%lu/%luus",
                        (unsigned long)u.tx, (unsigned long)u.pk * 4, (unsigned long)u.bmax, (unsigned long)u.qnow, (unsigned long)u.qmax,
                        (unsigned long)u.lat_avg, (unsigned long)u.lat_max, (unsigned long)u.wait_max, (unsigned long)u.gap_max,
                        (unsigned long)u.paced, (unsigned long)u.nak, (unsigned long)u.retry, (unsigned long)u.poll_avg, (unsigned long)u.poll_max);
            }
        }
        L_StatT = t; L_IsrCyc = L_IsrMax = L_IGap = 0; L_Isrs = L_Traps = L_Midi = L_Drop = L_Virq = L_Ack = 0;
        if (L_UnderInit) { L_UnderLast += L_Under; L_Under = 0; }
    }
    dbg_pump(&P);
}
void DBGL_Pump(void) { if (DBGL_On) { if (Cap_Dumping) cap_send(); dbg_pump(&P); } }

int DBGL_Status(char* t, int size)
{
    if (!DBGL_On) return P.base ? snprintf(t, (size_t)size, "DBG=OFF BASE=%X (SBDIAG DBG LEVEL 1 turns it on)", P.base) : snprintf(t, (size_t)size, "DBG=OFF");
    return snprintf(t, (size_t)size, "DBG=COM%d BASE=%X BAUD=%lu LVL=%d FIFO=%d SENT=%lu LOST=%lu%s", L_Com, P.base,
        (unsigned long)P.baud, DBGL_On, P.fifo, (unsigned long)P.sent, (unsigned long)P.lost, Cap_Dumping ? " DUMPING" : "");
}
int DBGL_SetLevel(int lvl)
{
    if (!P.base || lvl < 0 || lvl > 3) return DBGL_On;
    if (lvl == 3 && DBGL_On != 3) { Cap_W = 0; Cap_Dumping = 0; }
    DBGL_On = lvl; P.level = (uint8_t)lvl;
    if (lvl) DBGL_Line(0, 'I', "level %d", lvl);
    return DBGL_On;
}

int DBGL_Init(const char* spec, char* msg, int size)
{
    int com, lvl; uint16_t base; uint32_t baud;
    msg[0] = 0;
    if (DBGL_On) { dbg_sync(&P); DBGL_On = 0; }
    if (!spec || !*spec || !strcmp(spec, "0") || !strcasecmp(spec, "OFF")) { P.base = 0; return 0; }
    if (dbg_parse(spec, &com, &base, &baud, &lvl)) { snprintf(msg, (size_t)size, "Invalid /DBG:%s (COM1|COM2[,baud][,level 1-3])", spec); return -1; }
    if (com) base = _farpeekw(_dos_ds, 0x400 + (uint32_t)(com - 1) * 2);   /* DPMI_Load* needs DPMI_Init: not yet at option time */
    if (!base || dbg_uart_init(&P, base, baud, lvl))
    {
        if (com) snprintf(msg, (size_t)size, "/DBG: no serial port COM%d%s", com, base ? "" : " in the BIOS"); else snprintf(msg, (size_t)size, "/DBG: no serial port at %X", base);
        P.base = 0; return -2;
    }
    L_Com = com; L_T0 = DIAG_Tsc(); L_Tick0 = _farpeekl(_dos_ds, 0x46C); L_StatT = 0; Cap_W = 0; Cap_Dumping = 0;
    DBGL_On = lvl;
    snprintf(msg, (size_t)size, "Serial debug link: COM%d (%X), %lu baud, level %d%s", com, base,
        (unsigned long)P.baud, lvl, P.fifo == 16 ? "" : " (no FIFO)");
    put_line(1, "SB", 'I', msg);
    return 0;
}
