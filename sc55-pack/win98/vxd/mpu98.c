/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* mpu98.c - SC55W.VXD's MPU-401: UART mode + the intelligent-mode front end of DOS SBEMU beta.122 (#included by
 * sbcore.c; host test: test/mputest.c). Uses from sbcore.c: sh, cfg, ev_put, vxd_time_us, dbg, dbg_level, c_midi.
 *
 * Before w98.8 only FFh / 3Fh were answered and, outside UART mode, every data byte went to the synth: a game
 * that talks intelligent mode (version request, "want to send data" D0h-D7h, E0h-EFh with a parameter byte)
 * got parameter bytes played as notes. Now (as main.c MU_* in DOS):
 *  - UART mode (after 3Fh), or a game that never sent an intelligent-mode command: every data byte is MIDI;
 *  - intelligent mode: every command ACKed (FEh), A0h-AFh requests answered (ACh version 15h, ADh revision 01h,
 *    AFh tempo 100), D0h-D7h = one MIDI message follows (running status kept), DFh = a SysEx follows, the
 *    parameter byte of E0h..EFh swallowed, sequencer track data ignored (counted in sh->mpu_ignored);
 *  - status bit 6 (busy) while the event ring to SC55W is 3/4 full and SC55W is taking events (1 s without
 *    progress lets the game on), so a burst never overflows the ring (lost SysEx / note-offs).
 *
 * beta.131-awe.3 (Descent freeze in a Win98 DOS box): real-MPU timing on the status port.
 *  - A command's reply (ACK FEh, request data) shows on DSR (bit 7) only from the 2nd status read after the command,
 *    as a real MPU-401 answers after some microseconds. HMI SOS (Descent, HMIMDRV.386 mpu401.com) resets with
 *    "out FFh; loop up to 0FFFFFh WHILE DSR = 0; in data": on a card the first read still says "no data" and it is
 *    done; an ACK that is there at once kept it in the loop for 1,048,575 trapped reads (~6 s, PIC masked but IRQ 1),
 *    the freeze's start. Drivers that WAIT for the ACK (HMI's init, Miles, MIDPLAY) loop once more.
 *  - Busy (DRR) is held for at most MPU_DRR_READS status reads per byte (~a byte's wire time at 31250 baud): the
 *    ring back-pressure can slow a game, never park it (HMI waits 0FFFFh reads per byte, then resets the MPU).
 *  - A game spinning on DSR with nothing queued (HMI's reset waits up to 3FFFFh reads for data it never asked for)
 *    gets one active-sensing byte (FEh, what a keyboard on MIDI IN sends) after MPU_SENSE_READS reads in a row.
 *  - Reset (FFh) ends back-pressure until the next data byte. c_mpuspin counts long spins (dbg "S mpu spins=").
 *  Not done: VMM Release_Time_Slice from the trap on long spins - tried, Windows then gave the DOS box one time slice
 *  a second (4096 reads/s, HMIMPU 15 s per 64 bytes): it marks the VM idle. */
static u8  mpu_uart, mpu_intel, mpu_want, mpu_rs;
static u32 last_mpu_vm;
static int mpu_need;
static u8  mpu_q[8];
static int mpu_qn;
static u32 mpu_tail_seen, mpu_tail_time;
static u8  mpu_hide;           /* status reads before a queued reply shows on DSR */
static u8  mpu_nobusy;         /* after a reset: no back-pressure until the next data byte */
static u32 mpu_spin;           /* status reads in a row without another MPU-401 access */
static u32 c_mpuspin;          /* spins of MPU_SENSE_READS+ reads (dbg "S mpu spins=") */

#define MPU_DRR_READS   64u    /* ~320 us of trapped reads in a DOS box (real card: 1 byte = 320 us) */
#define MPU_SENSE_READS 2048u  /* ~10 ms of trapped reads */

static void mpu_queue(u8 v) { if (mpu_qn < 8) mpu_q[mpu_qn++] = v; }
static u8  lcd_pkt[150];      /* MPU command B0h: the front panel, read from the data port */
static int lcd_pos = 150;

static void mpu_reset_state(void)
{
    mpu_uart = 0; mpu_intel = 0; mpu_want = 0; mpu_rs = 0; mpu_qn = 0; lcd_pos = 150;
    mpu_hide = 0; mpu_nobusy = 0; mpu_spin = 0;
}

static int mpu_busy(void)
{
    u32 h, t, now;
    if (!sh || mpu_nobusy) return 0;
    h = sh->ev_head; t = sh->ev_tail;
    if (h - t < EVRING_SIZE * 3 / 4) return 0;
    now = vxd_time_us();
    if (t != mpu_tail_seen) { mpu_tail_seen = t; mpu_tail_time = now; return 1; }
    return now - mpu_tail_time < 1000000UL;   /* SC55W stopped taking events: do not hang the game */
}

static u8 mpu_read(u32 port)
{
    if (port == (u32)cfg.mpu_port + 1) {
        int data;
        u8 st;
        if (++mpu_spin == MPU_SENSE_READS) {
            c_mpuspin++;
            if (dbg_level) dbg('I', "MPU-401: vm=%08X reads the status %u times in a row (%s)", last_mpu_vm, MPU_SENSE_READS,
                               mpu_qn || mpu_intel ? "waits" : "nothing queued: active sensing FEh");
            if (!mpu_qn && lcd_pos >= 150 && !mpu_intel) { mpu_queue(0xFE); mpu_hide = 0; }   /* active sensing */
        }
        if (mpu_hide) { mpu_hide--; data = lcd_pos < 150; }
        else data = mpu_qn || lcd_pos < 150;
        st = (u8)(data ? 0x3F : 0xBF);            /* bit6 0 = ready, bit7 0 = data waiting */
        if (mpu_spin <= MPU_DRR_READS && mpu_busy()) st |= 0x40;
        return st;
    }
    mpu_spin = 0;
    mpu_hide = 0;                                 /* read without asking: it is there */
    if (lcd_pos < 150) return lcd_pkt[lcd_pos++];
    if (mpu_qn) {
        u8 v = mpu_q[0];
        int i;
        for (i = 1; i < mpu_qn; i++) mpu_q[i - 1] = mpu_q[i];
        mpu_qn--;
        return v;
    }
    return 0xFE;
}

static int mpu_len(u8 st) { return st >= 0xF0 ? (st == 0xF2 ? 2 : (st == 0xF1 || st == 0xF3) ? 1 : 0) : (st & 0xE0) == 0xC0 ? 1 : 2; }
static void mpu_out(u8 b) { c_midi++; ev_put(EV_MIDI, b, 0, 1); }   /* c = 1: from a DOS box (AWE line /AWEMPU:0) */

static void mpu_cmd(u8 v)
{
    if (v == 0xB0) {                     /* front panel (as SBEMU): 150 bytes follow on the data port */
        int i;
        for (i = 0; i < 150; i++) lcd_pkt[i] = sh ? sh->lcd[i] : 0;
        if (!sh || lcd_pkt[0] != 'L') { for (i = 0; i < 150; i++) lcd_pkt[i] = 0; lcd_pkt[0] = 'L'; }
        lcd_pos = 0;
        return;
    }
    lcd_pos = 150;
    if (v == 0xFF) {
        mpu_reset_state(); mpu_queue(0xFE); mpu_hide = 1; mpu_nobusy = 1;
        if (dbg_level) dbg('I', "MPU-401 reset (FFh) vm=%08X", last_mpu_vm);
        return;
    }
    mpu_queue(0xFE);                     /* every command is ACKed, in both modes (as SBEMU's MPU-401) */
    mpu_hide = 1;
    if (mpu_uart) return;
    if (v == 0x3F) { mpu_uart = 1; mpu_want = 0; if (dbg_level) dbg('I', "MPU-401 UART mode (3Fh) vm=%08X", last_mpu_vm); return; }
    if (v >= 0xA0 && v <= 0xAF) { mpu_queue((u8)(v == 0xAC ? 0x15 : v == 0xAD ? 0x01 : v == 0xAF ? 100 : 0)); return; }
    if (v >= 0xB0 && v <= 0xBF) return; /* SBEMU's own panel / load requests: not a game */
    mpu_intel = 1;
    if (v >= 0xD0 && v <= 0xD7) { mpu_want = 1; mpu_need = -1; }
    else if (v == 0xDF) mpu_want = 2;
    else if (v >= 0xE0 && v <= 0xEF) mpu_want = 3;
    else mpu_want = 0;
}

static void mpu_data(u8 b)
{
    mpu_nobusy = 0;
    if (mpu_uart || !mpu_intel) { mpu_out(b); return; }
    switch (mpu_want) {
    case 1:                              /* one MIDI message */
        if (b >= 0xF8) { mpu_out(b); return; }
        if (mpu_need < 0) {
            if (b >= 0x80) {
                mpu_want = 0;
                if (b >= 0xF0) { if (sh) sh->mpu_ignored++; return; }
                mpu_rs = b; mpu_need = mpu_len(b); mpu_out(b);
                if (mpu_need) mpu_want = 1;
                return;
            }
            if (!mpu_rs) { mpu_want = 0; if (sh) sh->mpu_ignored++; return; }
            mpu_need = mpu_len(mpu_rs); mpu_out(mpu_rs);   /* running status: sent again (Windows apps' MIDI shares the ring) */
        }
        mpu_out(b);
        if (--mpu_need <= 0) mpu_want = 0;
        return;
    case 2: mpu_out(b); if (b == 0xF7) mpu_want = 0; return;   /* SysEx up to F7 */
    case 3: mpu_want = 0; return;        /* the command's parameter byte */
    default: if (sh) sh->mpu_ignored++; return;   /* sequencer track data: not emulated */
    }
}

static void mpu_write(u32 port, u8 v)
{
    mpu_spin = 0;
    if (port == (u32)cfg.mpu_port + 1) mpu_cmd(v);
    else mpu_data(v);
}

/* once a second from dbg_stats (async time-out): status spins since the last S line */
static void mpu_dbg_stats(void)
{
    static u32 spin_last;
    if (c_mpuspin != spin_last) dbg('S', "mpu spins=%u (status read %u+ times in a row)", c_mpuspin - spin_last, MPU_SENSE_READS);
    spin_last = c_mpuspin;
}
