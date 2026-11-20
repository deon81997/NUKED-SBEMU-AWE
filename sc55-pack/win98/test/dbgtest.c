/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* dbgtest.c - native (gcc -m32) test of SC55W.VXD's serial debug link (vxd/dbg98.c, w98.9): line format, port
 * claim / refusal, ring overflow, level 3 capture dump, stats, and the ring-0 fault stub (run for real). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "../common/sc55wif.h"

#define BUILD_ID 4242
const char nsb_version_tag[] = "$VER: NUKED SB-EMU v1.0.0-test (SC55W.VXD) $";
static SC55W_SHARED *sh;
static SC55W_CONFIG cfg;
static u32 owner_vm = 0xC1000000u, joy_port = 0x201;
static struct { int valid; u32 addr, count, mode, base_addr, base_count; } dmach[2];

/* fake machine */
static char wire[1 << 20]; static int wire_n;
static int uart_ready = 1, uart_present = 1;
static u8 scratch, lcr;
static u32 fake_ms = 5000, fake_us;
static u32 vcomm_ret; static char vcomm_name[8]; static int vcomm_closed;
static u8 lowmem[0x500];
static u32 fakevm[2];
u32 vxd_port_in(u32 port)
{
    switch (port & 7) {
    case 7: return uart_present ? scratch : 0xFF;
    case 3: return uart_present ? lcr : 0xFF;
    case 2: return 0xC1;
    case 5: return uart_ready ? 0x60 : 0x00;
    }
    return 0;
}
void vxd_port_out(u32 port, u32 v)
{
    if ((port & 7) == 7) scratch = (u8)v;
    else if ((port & 7) == 3) lcr = (u8)v;
    else if ((port & 7) == 0 && !(lcr & 0x80) && wire_n < (int)sizeof wire) wire[wire_n++] = (char)v;
}
u32 vxd_sys_vm(void) { return (u32)fakevm; }
u32 vxd_time_us(void) { return fake_us += 7; }
u32 vxd_time_ms(void) { return fake_ms; }
static int armed;
void vxd_dbg_arm(u32 ms) { (void)ms; armed = 1; }
u32 vxd_vcomm_open(const char *n, u32 vm) { (void)vm; strcpy(vcomm_name, n); return vcomm_ret; }
void vxd_vcomm_close(u32 h) { (void)h; vcomm_closed++; }
void vxd_hang(void) { puts("hang called"); }
void vxd_fault_test(void) {}
void *vxd_heap_alloc(u32 n) { return calloc(1, n); }
u32 irq_save(void) { return 0; }
void irq_restore(u32 f) { (void)f; }
/* AWE line: the EMU8000 hooks dbg98.c calls (sbcore.c has the real ones) */
static int awe_port(u32 port) { u32 o = port - 0x620; return o < 4 || o - 0x400 < 4 || o - 0x800 < 4; }
static u32 awe_stats_calls, awe_stats_acc, awe_attach_calls;
static void dbg(char kind, const char *f, ...);
static void awe_dbg_attach(u32 st) { (void)st; awe_attach_calls++; }
static void mpu_dbg_stats(void) { }
static void awe_dbg_stats(u32 acc) { awe_stats_calls++; awe_stats_acc = acc; if (acc) dbg('S', "awe vm=00000000 ports=%u voices=0/32 ramw=0 ram=512K", acc); }

#include "../vxd/dbg98.c"

static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }
static void drain(void) { int i; for (i = 0; i < 100000 && dbg_head != dbg_tail; i++) sc_dbg_timer(); }
static int lines_matching(const char *re)
{
    regex_t r; int n = 0; char *s, *e, buf[512];
    regcomp(&r, re, REG_EXTENDED | REG_NEWLINE);
    wire[wire_n] = 0;
    for (s = wire; (e = strstr(s, "\r\n")); s = e + 2) {
        int len = (int)(e - s) < 511 ? (int)(e - s) : 511;
        memcpy(buf, s, len); buf[len] = 0;
        if (!regexec(&r, buf, 0, NULL, 0)) n++;
    }
    regfree(&r);
    return n;
}

int main(void)
{
    char msg[160], t[64];
    u32 r, i;
    fakevm[1] = (u32)lowmem;
    lowmem[0x400] = 0xF8; lowmem[0x401] = 0x03;           /* BIOS: COM1 = 3F8 */
    load_ms = 1000;

    fmt(t, sizeof t, "%08X|%u.%03u|%d|%s|%c|%4u|%x", 0x1Au, 12u, 7u, -5, "ab", 'Z', 42u, 0xBEEFu);
    check(!strcmp(t, "0000001A|12.007|-5|ab|Z|  42|beef"), "formatter");

    vcomm_ret = (u32)-2;                                   /* IE_OPEN: a serial mouse has it */
    r = dbg_open(1, 0, 1, msg, sizeof msg);
    check(r == 1 && strstr(msg, "COM1 (3F8) is in use in Windows") && !dbg_base && wire_n == 0, "refused when Windows has COM1 open");
    vcomm_ret = (u32)-1; uart_present = 0;                 /* IE_BADID + no UART */
    r = dbg_open(2, 0, 1, msg, sizeof msg);
    check(r == 2 && strstr(msg, "no serial port (UART) at COM2 2F8") && !dbg_base, "no UART -> refused");
    uart_present = 1;
    r = dbg_open(5, 0, 1, msg, sizeof msg);
    check(r == 3, "COM5 = bad value");

    vcomm_ret = 0xC1234560u;
    r = dbg_open(1, 0, 1, msg, sizeof msg);
    drain();
    check(r == 0 && dbg_owned && !strcmp(vcomm_name, "COM1"), "COM1 claimed through VCOMM");
    check(lines_matching("^4\\.000 VXD I Serial debug link: COM1 \\(3F8\\), 115200 baud, level 1, port claimed from Windows$") == 1, "first line as DOS");
    check(lines_matching("^4\\.000 VXD I VxD loaded: NUKED SB-EMU v1.0.0-test \\(SC55W.VXD\\) build 4242, interface 11, 4\\.000 s ago$") == 1, "VxD load line");

    fake_ms += 1234;
    dbg_raw(0, "SC55W", 'I', "synth: SC-55 mk1 started");
    drain();
    check(lines_matching("^5\\.234 SC55W I synth: SC-55 mk1 started$") == 1, "helper line through the VxD");

    c_sb += 100; c_mpu += 7; c_midi += 3;
    fake_ms += 1000;
    sc_dbg_timer(); drain();
    check(lines_matching("^6\\.234 VXD S vm=C1000000 sb=100 mpu=7 opl=0 joy=0 awe=0 virq=0 ack=0 midi=3 dma=0 ev=0/32768 evlost=0 pcmlost=0 lost=0$") == 1, "stats line once a second");
    sh = calloc(1, sizeof *sh);
    sh->ev_overflow = 17;
    fake_ms += 1000; sc_dbg_timer(); drain();
    check(lines_matching("VXD E event ring full: 17 events lost") == 1, "event ring lost -> E line");

    dbg('I', "DOS box start vm=%08X", 0xC2000000u);
    drain();
    check(lines_matching("VXD I DOS box start vm=C2000000$") == 1, "DOS box line");

    cfg.sb_port = 0x220; cfg.mpu_port = 0x330;
    /* w98.10: the S-line port counts are one add in sc_in/sc_out; the IO stub calls sc_dbg_capture only at level 3 */
    check(dbg_level == 1 && sc_dbg_cap == 0, "level 1: no capture call per trapped access");
    /* AWE line: awe= = the trap counter (one add in awe_in8/awe_out8 per byte, sc_iow per word); capture/capw count nothing */
    { u32 a = c_awe, sb = c_sb, n = awe_stats_calls;
      sc_dbg_capture(0x12, 0x620, 1); sc_dbg_capw(0xBEEF, 0xA20, 1);
      check(c_awe == a && c_sb == sb, "capture/capw add no counts (counted in the trap paths)");
      c_awe += 3 + 2;                              /* as awe_in8/awe_out8 (620, A22, E23) + sc_iow (A20, 622) */
      fake_ms += 1000; sc_dbg_timer(); drain();
      check(lines_matching("VXD S vm=C1000000 sb=[0-9]+ mpu=[0-9]+ opl=[0-9]+ joy=[0-9]+ awe=5 virq=") == 1 && awe_stats_calls == n + 1 &&
            awe_stats_acc == 5 && lines_matching("^[0-9.]+ VXD S awe vm=00000000 ports=5 voices=0/32 ramw=0 ram=512K$") == 1,
            "S line awe=5 from the trap counter + EMU8000 stats line"); }

    /* UART stuck (cable / flow): the ring fills, lines are dropped and counted, nothing overruns */
    uart_ready = 0;
    for (i = 0; i < 2000; i++) dbg('I', "filler line %u xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", i);
    check(dbg_lost > 0 && dbg_head - dbg_tail <= DBG_RING, "ring full: lines dropped, counted");
    uart_ready = 1; drain();
    check(dbg_head == dbg_tail, "ring drains");

    /* level 3: capture + dump */
    r = dbg_open(1, 0, 3, msg, sizeof msg);
    check(sc_dbg_cap == 1, "level 3: the IO stub calls the capture");
    for (i = 0; i < 5000; i++) cap_add(0x220 + (i & 15), i & 0xFF, i & 1);
    wire_n = 0;
    r = dbg_cmd(DBGCMD_DUMP, msg, sizeof msg);
    for (i = 0; i < 100000 && (cap_dumping || dbg_head != dbg_tail); i++) sc_dbg_timer();
    check(r == 0 && lines_matching("^T [0-9]+ [io] 22[0-9A-F] [0-9A-F]{2}$") == 4096 && lines_matching("VXD I capture end$") == 1,
          "level 3: 4096 T records + capture end");
    /* level 3, AWE line: an EMU8000 word = its two bytes (port, port+1) in the capture */
    cap_head = 0; cap_dumping = 0;
    sc_dbg_capw(0xBEEF, 0xA20, 1);
    check(cap_head == 2 && cap[0].port == 0xA20 && cap[0].val == 0xEF && cap[1].port == 0xA21 && cap[1].val == 0xBE && cap[1].out == 1,
          "level 3: EMU8000 word captured as two bytes");

    dbg_close();
    check(vcomm_closed == 1, "after close: VCOMM port closed");

    printf("dbgtest: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
