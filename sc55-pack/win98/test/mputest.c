/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* mputest.c - native (gcc -m32) test of SC55W.VXD's MPU-401 front end (vxd/mpu98.c) against the MPU-401 code of real
 * DOS drivers, loop for loop: HMI SOS (Descent 1.4 HMIMDRV.386 mpu401.com: init, send, reset), a Miles-style reset
 * (wait for the ACK), intelligent mode (ACh/ADh), SBEMU's panel (B0h) and ring back-pressure. beta.131-awe.3: Descent's reset
 * spun 1,048,575 trapped reads after FFh (ACK there at once) - now 1, as on a card. Run: test/mputest.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../common/sc55wif.h"

static SC55W_SHARED *sh;
static SC55W_CONFIG cfg;
static u32 dbg_level = 1, c_midi, now_us;
static int n_dbg; static char last_dbg[200];
static u32 vxd_time_us(void) { return now_us; }
static void dbg(char kind, const char *f, ...)
{
    va_list ap; va_start(ap, f); vsnprintf(last_dbg, sizeof last_dbg, f, ap); va_end(ap); n_dbg++; (void)kind;
}
static void ev_put(u8 type, u8 a, u8 b, u8 c)
{
    u32 h = sh->ev_head;
    if (h - sh->ev_tail >= EVRING_SIZE) { sh->ev_overflow++; return; }
    sh->ev[h & (EVRING_SIZE - 1)].type = type; sh->ev[h & (EVRING_SIZE - 1)].a = a;
    (void)b; (void)c; sh->ev_head = h + 1;
}
#include "../vxd/mpu98.c"

static u32 reads;
static u8 in(u32 p) { reads++; now_us += 5; return mpu_read(p); }
static void out(u32 p, u8 v) { now_us += 5; mpu_write(p, v); }
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* HMI mpu401.com 0x183 init: returns error 0/8, *ack = data read after FFh */
static int hmi_init(u8 *ack, u32 *dsr_loops)
{
    u32 ecx; int err = 0;
    out(0x331, 0xFF);
    for (ecx = 0xFFFF; ecx; ecx--) if (!(in(0x331) & 0x80)) break;
    if (!ecx) err = 8;
    *dsr_loops = 0xFFFF - ecx;
    *ack = in(0x330);
    for (ecx = 0xFFFF; ecx; ecx--) if (!(in(0x331) & 0x40)) break;
    if (!ecx) err = 8;
    out(0x331, 0x3F);
    for (ecx = 0xFFFF; ecx; ecx--) in(0x331);
    in(0x330);
    return err;
}
/* 0x297 send: max DRR loops of n bytes */
static u32 hmi_send(const u8 *b, int n)
{
    u32 mx = 0, ecx;
    while (n--) {
        for (ecx = 0xFFFF; ecx; ecx--) if (!(in(0x331) & 0x40)) break;
        if (0xFFFF - ecx > mx) mx = 0xFFFF - ecx;
        out(0x330, *b++);
    }
    return mx;
}
/* 0x21d reset: pre = loops waiting for DSR before FFh, post = loops WHILE DSR after it, *r = byte read */
static void hmi_reset(u32 *pre, u32 *post, u8 *r)
{
    u32 ecx;
    for (ecx = 0x3FFFF; ecx; ecx--) if (!(in(0x331) & 0x80)) break;
    *pre = 0x3FFFF - ecx;
    out(0x331, 0xFF);
    for (ecx = 0xFFFFF; ecx; ecx--) if (in(0x331) & 0x80) break;
    *post = 0xFFFFF - ecx;
    *r = in(0x330);
}
/* Miles / MIDPLAY style: FFh, wait for DSR (bounded), read, want FEh */
static int wait_ack(u8 cmd, u32 *loops)
{
    u32 n;
    out(0x331, cmd);
    for (n = 0; n < 100000; n++) if (!(in(0x331) & 0x80)) break;
    *loops = n;
    return in(0x330);
}

int main(void)
{
    static const u8 notes[8] = { 0x99, 0x24, 0x40, 0x89, 0x24, 0, 0x99, 0x26 };
    u8 ack, r; u32 l, pre, post, s; int i;
    sh = calloc(1, sizeof *sh);
    cfg.mpu_port = 0x330;
    mpu_reset_state();

    /* HMI three times: init, 64 bytes, reset (Descent: start, play, song change) */
    for (i = 0; i < 3; i++) {
        reads = 0;
        CHECK(hmi_init(&ack, &l) == 0, "HMI init error");
        CHECK(ack == 0xFE && l <= 2, "HMI init: ACK %02X after %u loops", ack, l);
        CHECK(mpu_uart, "UART mode after 3Fh");
        s = hmi_send(notes, 8);
        CHECK(s == 0, "send: DRR loops %u with an empty ring", s);
        hmi_reset(&pre, &post, &r);
        CHECK(post <= 1, "HMI reset: %u loops while DSR after FFh (real MPU: 0-1; awe.10: 1048575)", post);
        CHECK(r == 0xFE, "HMI reset: read %02X", r);
        CHECK(pre <= MPU_SENSE_READS, "HMI pre-reset wait %u loops (cap %u)", pre, MPU_SENSE_READS);
        CHECK(reads < 0xFFFF + 4 * MPU_SENSE_READS, "HMI cycle %u reads", reads);
    }
    /* the next init after a reset whose pre-wait got the active-sensing byte: no stale byte left */
    CHECK(hmi_init(&ack, &l) == 0 && ack == 0xFE, "init after reset: %02X", ack);

    /* Miles-style: wait for the ACK after FFh and after 3Fh */
    CHECK(wait_ack(0xFF, &l) == 0xFE && l == 1, "FFh ACK after %u loops", l);
    CHECK(wait_ack(0x3F, &l) == 0xFE && l == 1, "3Fh ACK after %u loops", l);
    /* FFh twice in UART mode (drivers that expect no ACK from a Roland in UART mode send two) */
    out(0x331, 0xFF); out(0x331, 0xFF);
    CHECK(wait_ack(0xFF, &l) == 0xFE, "third FFh ACK");
    CHECK(in(0x330) == 0xFE, "empty data port reads FEh");
    /* reading the data port without asking the status: the reply is there */
    out(0x331, 0xFF); CHECK(in(0x330) == 0xFE, "direct read after FFh");
    CHECK(in(0x331) & 0x80, "nothing left after the direct read");

    /* intelligent mode: ACh -> FE then 15h, ADh -> FE then 01h */
    out(0x331, 0xFF); in(0x330);
    CHECK(wait_ack(0xAC, &l) == 0xFE, "ACh ACK");
    CHECK(!(in(0x331) & 0x80) && in(0x330) == 0x15, "ACh version 15h");
    CHECK(wait_ack(0xAD, &l) == 0xFE && in(0x330) == 0x01, "ADh revision 01h");
    /* intelligent mode: a long status spin never injects a byte (the game's sequencer protocol) */
    out(0x331, 0xD0); in(0x330);
    for (l = 0; l < 3 * MPU_SENSE_READS; l++) CHECK((in(0x331) & 0x80), "intelligent-mode spin got data");
    out(0x331, 0xFF); in(0x330); out(0x331, 0x3F); in(0x330);

    /* panel B0h: 150 bytes at once ('L' first) */
    out(0x331, 0xB0);
    CHECK(!(in(0x331) & 0x80) && in(0x330) == 'L', "B0h panel");
    for (i = 1; i < 150; i++) in(0x330);

    /* back-pressure: ring 3/4 full and SC55W taking events -> busy for at most MPU_DRR_READS reads per byte */
    sh->ev_tail += 5; sh->ev_head = sh->ev_tail + EVRING_SIZE * 3 / 4 + 10;
    s = hmi_send(notes, 8);
    CHECK(s >= 1 && s <= MPU_DRR_READS, "send with the ring 3/4 full: %u DRR loops (cap %u)", s, MPU_DRR_READS);
    c_midi = 0; s = hmi_send(notes, 8);
    CHECK(c_midi == 8, "all bytes went to the ring (%u)", c_midi);
    /* a reset during back-pressure: no busy until the next data byte (HMI's init must not time out) */
    out(0x331, 0xFF); in(0x330);
    for (l = 0; l < 10; l++) CHECK(!(in(0x331) & 0x40), "busy after a reset");
    sh->ev_tail++;   /* SC55W takes one */
    s = hmi_send(notes, 1);
    CHECK(s <= MPU_DRR_READS, "after reset: %u DRR loops", s);
    /* ring full: bytes dropped and counted, never a hang */
    sh->ev_head = sh->ev_tail + EVRING_SIZE;
    l = sh->ev_overflow; hmi_send(notes, 8);
    CHECK(sh->ev_overflow == l + 8, "full ring: %u lost", sh->ev_overflow - l);

    CHECK(n_dbg > 0 && c_mpuspin > 0, "spin counted (%u) + dbg line (%s)", c_mpuspin, last_dbg);
    printf("%s (%d checks)\n", fails ? "FAIL" : "PASS", checks);
    return fails != 0;
}
