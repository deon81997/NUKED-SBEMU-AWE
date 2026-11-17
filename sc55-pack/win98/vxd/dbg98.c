/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/*
 * dbg98.c - serial debug link of SC55W.VXD (w98.9), the Windows 98 side of DOS SBEMU /DBG (beta.127).
 * #included by sbcore.c (one object file).
 *
 * Same lines as DOS: "<seconds.ms> <source> <kind> <text>\r\n", source VXD (this VxD) or SC55W (the helper,
 * through IOCTL_SC55W_DBG_LINE), kinds E error, ! fatal, I event, S stats once a second, F per frame (level 2),
 * T capture record (level 3). The clock starts when the VxD is loaded.
 *
 * The VxD writes the UART itself, from ring 0, polled (no UART interrupt): a 16 KB ring, filled by whoever
 * makes a line, emptied one FIFO load (16 bytes) at a time by the maker and by an async time-out that runs at
 * interrupt time even while Windows is stuck in a critical section, and goes on after a driver fault (tested:
 * SC55W /DBG:FAULT). E and ! lines wait until they are on the wire. (Hook_VMM_Fault answers carry for this
 * dynamic VxD in Windows 98, so there is no extra line before a blue screen.) The port is claimed from Windows with VCOMM_OpenComm, so Windows programs, VCOMM clients and DOS
 * boxes cannot use it while the link runs; a port Windows already has open is refused (status 1 + message).
 * Off (no /DBG) = no line is ever built: every hook tests dbg_level first.
 */
#include <stdarg.h>

extern u32  __cdecl vxd_time_ms(void);
extern void __cdecl vxd_dbg_arm(u32 ms);
extern u32  __cdecl vxd_vcomm_open(const char *name, u32 vmid);
extern void __cdecl vxd_vcomm_close(u32 h);
extern void __cdecl vxd_hang(void);
extern void __cdecl vxd_fault_test(void);
extern void * __cdecl vxd_heap_alloc(u32 n);

u32 irq_save(void);
#pragma aux irq_save = "pushfd" "pop eax" "cli" value [eax];
void irq_restore(u32 f);
#pragma aux irq_restore = "push eax" "popfd" parm [eax];

#define DBG_RING 16384u
#define CAP_N    4096u
static u32  dbg_level, dbg_base, dbg_baud, dbg_fifo, dbg_port, dbg_vcomm, dbg_owned;
static u32  dbg_head, dbg_tail, dbg_lost, dbg_sent, dbg_next_s;
static char *dbg_ring;                    /* VMM heap, at the first /DBG (the VxD image stays as small as before) */
static u32  load_ms;                      /* when Windows loaded the VxD */
static int  dbg_async;                    /* in the async time-out: never wait for the UART */
/* counters for the S / F lines: port counts from sc_dbg_capture (link on only), virq/midi/dma bytes always (rare) */
static u32  c_sb, c_mpu, c_opl, c_joy, c_awe, c_virq, c_ack, c_midi, c_dmab;
static struct { u32 sb, mpu, opl, joy, awe, virq, ack, midi, evlost, pcmlost, dmab, lost; } s_last, f_last;
/* level 3: the last CAP_N trapped port accesses, sent on SC55W /DBG:DUMP */
static struct { u32 us; u16 port; u8 val, out; } *cap;
static u32  cap_head, cap_dump, cap_dumping;

/* ---- tiny formatter: %s %c %d %u %x %X with 0 / width and l (ignored: all 32-bit) ---- */
static int vfmt(char *o, int max, const char *f, va_list ap)
{
    int n = 0;
    while (*f && n < max - 1) {
        char c = *f++, buf[12];
        int zero = 0, w = 0, len = 0, neg = 0;
        u32 v, base;
        const char *s;
        if (c != '%') { o[n++] = c; continue; }
        if (*f == '0') { zero = 1; f++; }
        while (*f >= '0' && *f <= '9') w = w * 10 + (*f++ - '0');
        while (*f == 'l') f++;
        c = *f ? *f++ : 0;
        if (c == 's') {
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && n < max - 1) o[n++] = *s++;
            continue;
        }
        if (c == 'c') { o[n++] = (char)va_arg(ap, int); continue; }
        if (c != 'd' && c != 'u' && c != 'x' && c != 'X') { o[n++] = c; continue; }
        v = va_arg(ap, u32);
        base = (c == 'x' || c == 'X') ? 16 : 10;
        if (c == 'd' && (s32)v < 0) { neg = 1; v = (u32)-(s32)v; }
        do { u32 d = v % base; buf[len++] = (char)(d < 10 ? '0' + d : (c == 'x' ? 'a' : 'A') + d - 10); v /= base; } while (v && len < 11);
        if (neg) { if (zero) { if (n < max - 1) o[n++] = '-'; } else buf[len++] = '-'; w--; }
        while (w > len && n < max - 1) { o[n++] = zero ? '0' : ' '; w--; }
        while (len && n < max - 1) o[n++] = buf[--len];
    }
    o[n] = 0;
    return n;
}
static int fmt(char *o, int max, const char *f, ...)
{
    va_list ap; int n;
    va_start(ap, f); n = vfmt(o, max, f, ap); va_end(ap);
    return n;
}

/* ---- UART ---- */
static void dbg_pump(void)
{
    u32 f, n = 0;
    if (!dbg_base) return;
    f = irq_save();                                   /* one consumer: the maker or the async time-out */
    if (dbg_head != dbg_tail && (vxd_port_in(dbg_base + 5) & 0x20)) {
        while (n < dbg_fifo && dbg_tail != dbg_head) {
            vxd_port_out(dbg_base, (u8)dbg_ring[dbg_tail & (DBG_RING - 1)]);
            dbg_tail++; n++;
        }
        dbg_sent += n;
    }
    irq_restore(f);
}

/* wait until the ring is empty: at most ~2x the wire time + ~50 ms of LSR reads (~1 us each) */
static void dbg_sync(void)
{
    u32 spin;
    if (!dbg_base) return;
    spin = ((dbg_head - dbg_tail) + 16) * (20000000UL / (dbg_baud ? dbg_baud : 9600)) / 16 + 50000UL;
    while (dbg_head != dbg_tail && spin--) dbg_pump();
    spin = 200000UL;
    while (spin-- && !(vxd_port_in(dbg_base + 5) & 0x40)) {}
}

static void dbg_put(const char *s, u32 len)
{
    u32 f, h, i;
    f = irq_save();
    h = dbg_head;
    if (DBG_RING - (h - dbg_tail) < len + 1) { dbg_lost++; irq_restore(f); return; }
    for (i = 0; i < len; i++) dbg_ring[(h + i) & (DBG_RING - 1)] = s[i];
    dbg_head = h + len;
    irq_restore(f);
}

static u32 dbg_now(void) { return vxd_time_ms() - load_ms; }

/* one line: "<s.ms> <src> <kind> <text>". sync = wait until it left the cable (E, !) */
static void dbg_raw(int sync, const char *src, char kind, const char *text)
{
    char b[256];
    u32 ms = dbg_now();
    int n = fmt(b, 32, "%u.%03u %s ", ms / 1000, ms % 1000, src);
    if (kind) { b[n++] = kind; b[n++] = ' '; }
    while (*text && n < (int)sizeof b - 3) b[n++] = *text++;
    while (n && (b[n - 1] == '\n' || b[n - 1] == '\r')) n--;
    b[n++] = '\r'; b[n++] = '\n';
    dbg_put(b, (u32)n);
    if ((sync || kind == 'E' || kind == '!') && !dbg_async) dbg_sync(); else { dbg_pump(); vxd_dbg_arm(2); }
}

static void dbg(char kind, const char *f, ...)
{
    char t[230]; va_list ap;
    if (!dbg_level) return;
    va_start(ap, f); vfmt(t, sizeof t, f, ap); va_end(ap);
    dbg_raw(0, "VXD", kind, t);
}

/* ---- stats (S) once a second, per-frame (F, level 2), capture dump (T, level 3) ---- */
static void dbg_stats(void)
{
    u32 evl = sh ? sh->ev_overflow : 0, pcl = sh ? sh->pcm_overflow : 0, evq = sh ? sh->ev_head - sh->ev_tail : 0;
    dbg('S', "vm=%08X sb=%u mpu=%u opl=%u joy=%u awe=%u virq=%u ack=%u midi=%u dma=%u ev=%u/%u evlost=%u pcmlost=%u lost=%u",
        owner_vm, c_sb - s_last.sb, c_mpu - s_last.mpu, c_opl - s_last.opl, c_joy - s_last.joy, c_awe - s_last.awe, c_virq - s_last.virq,
        c_ack - s_last.ack, c_midi - s_last.midi, c_dmab - s_last.dmab, evq, EVRING_SIZE, evl - s_last.evlost,
        pcl - s_last.pcmlost, dbg_lost - s_last.lost);
    awe_dbg_stats(c_awe - s_last.awe);          /* AWE line: EMU8000 voices / RAM (sbcore.c), only while used */
    if (evl != s_last.evlost)
        dbg('E', "event ring full: %u events lost (SC55W did not take them in time)", evl - s_last.evlost);
    if (pcl != s_last.pcmlost)
        dbg('E', "PCM ring full: %u frames lost", pcl - s_last.pcmlost);
    s_last.sb = c_sb; s_last.mpu = c_mpu; s_last.opl = c_opl; s_last.joy = c_joy; s_last.awe = c_awe; s_last.virq = c_virq;
    s_last.ack = c_ack; s_last.midi = c_midi; s_last.dmab = c_dmab; s_last.evlost = evl; s_last.pcmlost = pcl;
    s_last.lost = dbg_lost;
}

static void dbg_frame(int idx)
{
    if (dbg_level != 2) return;
    dbg('F', "sb=%u mpu=%u opl=%u joy=%u virq=%u ack=%u dma%u=%05X/%X", c_sb - f_last.sb, c_mpu - f_last.mpu,
        c_opl - f_last.opl, c_joy - f_last.joy, c_virq - f_last.virq, c_ack - f_last.ack,
        idx ? (u32)cfg.dma16 : (u32)cfg.dma8, dmach[idx].addr & 0xFFFFF, dmach[idx].count);
    f_last.sb = c_sb; f_last.mpu = c_mpu; f_last.opl = c_opl; f_last.joy = c_joy; f_last.virq = c_virq; f_last.ack = c_ack;
}

u32 sc_dbg_cap;                          /* 1 = link on: the IO stub calls sc_dbg_capture (off: one compare per trap) */
static void cap_add(u32 port, u32 val, int out)
{
    u32 i;
    if (dbg_level != 3 || !cap || cap_dumping) return;
    i = cap_head++ & (CAP_N - 1);
    cap[i].us = vxd_time_us(); cap[i].port = (u16)port; cap[i].val = (u8)val; cap[i].out = (u8)out;
}

static void cap_pump(void)
{
    char b[48];
    u32 first = cap_head > CAP_N ? cap_head - CAP_N : 0;
    while (cap_dumping && DBG_RING - (dbg_head - dbg_tail) > 64) {
        u32 i = first + cap_dump, n;
        if (i >= cap_head) {
            cap_dumping = 0;
            n = fmt(b, sizeof b, "%u.%03u VXD I capture end\r\n", dbg_now() / 1000, dbg_now() % 1000);
            dbg_put(b, n);
            break;
        }
        i &= CAP_N - 1;
        n = fmt(b, sizeof b, "T %u %s %03X %02X\r\n", cap[i].us - cap[first & (CAP_N - 1)].us, cap[i].out ? "o" : "i",
                (u32)cap[i].port, (u32)cap[i].val);
        dbg_put(b, n);
        cap_dump++;
    }
}

/* every trapped byte while the link is on: the S line's port counts (and the level 3 capture) */
void __cdecl sc_dbg_capture(u32 val, u32 port, u32 out)
{
    if (port - cfg.sb_port < 16) { c_sb++; if (!out && (port & 0xE) == 0xE) c_ack++; }
    else if (port - 0x388 < 4) c_opl++;
    else if (port - cfg.mpu_port < 2) c_mpu++;
    else if (port == joy_port) c_joy++;
    else if (awe_port(port)) c_awe++;           /* AWE line: EMU8000 620-623/A20-A23/E20-E23 (+ base) */
    if (dbg_level == 3) cap_add(port, val, (int)out);
}

/* AWE line: an EMU8000 word access (IO_Stub io_w, link on): one count, captured as its two bytes */
void __cdecl sc_dbg_capw(u32 val, u32 port, u32 out)
{
    c_awe++;
    if (dbg_level == 3) { cap_add(port, val & 0xFF, (int)out); cap_add(port + 1, (val >> 8) & 0xFF, (int)out); }
}

/* the async time-out: interrupt time - only the ring, the UART and VTD time here */
void __cdecl sc_dbg_timer(void)
{
    u32 now;
    if (!dbg_base) return;
    dbg_async = 1;
    dbg_pump();
    if (cap_dumping) cap_pump();
    now = dbg_now();
    if (dbg_level && (s32)(now - dbg_next_s) >= 0) { dbg_next_s = now + 1000; dbg_stats(); }
    dbg_async = 0;
    vxd_dbg_arm(dbg_head != dbg_tail ? 2 : 250);
}

/* ---- open / close the port ---- */
static void dbg_close(void)
{
    if (dbg_base && dbg_level) { dbg_raw(1, "VXD", 'I', "serial debug link off"); }
    dbg_level = 0;
    sc_dbg_cap = 0;
    dbg_base = 0;
    if (dbg_owned) { vxd_vcomm_close(dbg_vcomm); dbg_owned = 0; dbg_vcomm = 0; }
    dbg_port = 0;
}

static int uart_init(u32 base, u32 baud)
{
    u32 div = 115200UL / (baud ? baud : 115200UL), iir;
    if (!div) div = 1;
    vxd_port_out(base + 7, 0x5A);
    if (vxd_port_in(base + 7) != 0x5A) { vxd_port_out(base + 3, 0x03); if (vxd_port_in(base + 3) != 0x03) return -1; }
    vxd_port_out(base + 1, 0);                        /* no UART interrupts */
    vxd_port_out(base + 3, 0x80); vxd_port_out(base + 0, div & 0xFF); vxd_port_out(base + 1, div >> 8);
    vxd_port_out(base + 3, 0x03);
    vxd_port_out(base + 2, 0x07);
    iir = vxd_port_in(base + 2);
    vxd_port_out(base + 4, 0x03);                     /* DTR + RTS, OUT2 off */
    dbg_fifo = ((iir & 0xC0) == 0xC0) ? 16 : 1;
    dbg_baud = 115200UL / div;
    return 0;
}

/* port 1..4, level 1..3 (0 = off). 0 = on, 1 = Windows uses the port, 2 = no UART, 3 = bad args */
static u32 dbg_open(u32 port, u32 baud, u32 level, char *msg, u32 msgmax)
{
    static const u16 def[4] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};
    char name[5] = {'C', 'O', 'M', '0', 0};
    u32 base, h;
    const char *how;
    msg[0] = 0;
    if (!port) { dbg_close(); fmt(msg, (int)msgmax, "off"); return 0; }
    if (port > 4 || level > 3 || (baud && (baud < 300 || baud > 115200))) { fmt(msg, (int)msgmax, "bad /DBG value"); return 3; }
    if (!level) level = 1;
    if (port == dbg_port && dbg_base) {               /* already on: new level / baud */
        if (baud && baud != dbg_baud) uart_init(dbg_base, baud);
        dbg_level = level;
        if (level == 3 && !cap) cap = vxd_heap_alloc(CAP_N * sizeof *cap);
        sc_dbg_cap = 1;
        dbg('I', "level %u, %u baud", level, dbg_baud);
        fmt(msg, (int)msgmax, "COM%u: level %u", port, level);
        return 0;
    }
    dbg_close();
    if (!dbg_ring) dbg_ring = (char *)vxd_heap_alloc(DBG_RING);
    if (!dbg_ring) { fmt(msg, (int)msgmax, "no memory for the debug link"); return 3; }
    name[3] = (char)('0' + port);
    base = *(u16 *)(*(u32 *)(vxd_sys_vm() + 4) + 0x400 + 2 * (port - 1));   /* BIOS data area */
    if (!base) base = def[port - 1];
    h = vxd_vcomm_open(name, 0xFFFFFFFFUL);
    if ((s32)h < 0 && (s32)h >= -32 && (s32)h != -1) {   /* IE_OPEN (-2), IE_HARDWARE (-10)...: someone has it */
        fmt(msg, (int)msgmax, "%s (%X) is in use in Windows (error %d: a serial mouse, a modem, a program or a DOS box "
            "has it). Free it or use the other COM port.", name, base, (s32)h);
        return 1;
    }
    if ((s32)h > 0 || (s32)h < -32) { dbg_vcomm = h; dbg_owned = 1; how = "claimed from Windows"; }
    else how = "not known to Windows";                 /* IE_BADID: no port driver - Windows never uses it */
    if (uart_init(base, baud)) {
        if (dbg_owned) { vxd_vcomm_close(dbg_vcomm); dbg_owned = 0; }
        fmt(msg, (int)msgmax, "no serial port (UART) at %s %X: switched off in the BIOS setup?", name, base);
        return 2;
    }
    dbg_head = dbg_tail = dbg_lost = dbg_sent = 0;
    dbg_base = base; dbg_port = port; dbg_level = level;
    if (level == 3 && !cap) cap = vxd_heap_alloc(CAP_N * sizeof *cap);
    sc_dbg_cap = 1;
    s_last.sb = c_sb; s_last.mpu = c_mpu; s_last.opl = c_opl; s_last.joy = c_joy; s_last.awe = c_awe; s_last.virq = c_virq;
    s_last.ack = c_ack; s_last.midi = c_midi; s_last.dmab = c_dmab; s_last.lost = 0;
    s_last.evlost = sh ? sh->ev_overflow : 0; s_last.pcmlost = sh ? sh->pcm_overflow : 0;
    f_last = s_last;
    dbg_next_s = dbg_now() + 1000;
    dbg('I', "Serial debug link: %s (%X), %u baud, level %u, port %s", name, base, dbg_baud, level, how);
    {
        char v[64];
        int i = 0;
        const char *t = nsb_version_tag + 6;                /* "$VER: NUKED SB-EMU <ver> (SC55W.VXD) $" */
        while (t[i] && t[i] != '$' && i < 63) { v[i] = t[i]; i++; }
        while (i && v[i - 1] == ' ') i--;
        v[i] = 0;
        dbg('I', "VxD loaded: %s build %u, interface %u, %u.%03u s ago", v, (u32)BUILD_ID, (u32)SC55W_IF_VERSION,
            dbg_now() / 1000, dbg_now() % 1000);
    }
    fmt(msg, (int)msgmax, "%s (%X), %u baud, level %u, port %s", name, base, dbg_baud, level, how);
    return 0;
}

/* what ATTACH trapped (I) and what it could not (E) */
static void dbg_attach(u32 st)
{
    dbg('I', "attach: SB %X IRQ %u DMA %u/%u type %u, OPL %s, MPU %X, game port %X: status %05X", (u32)cfg.sb_port,
        (u32)cfg.irq, (u32)cfg.dma8, (u32)cfg.dma16, (u32)cfg.sbtype, cfg.opl ? "on" : "off", (u32)cfg.mpu_port,
        (cfg.joy & 1) ? joy_port : 0, st);
    dbg('I', "port traps: SB %s, OPL %s, MPU %s, IRQ %s, DMA %s, DMA16 %s, joystick %s, INT 15h %s, INT 21h %s",
        (st & ST_OK_SB) ? "ok" : "-", (st & ST_OK_OPL) ? "ok" : "-", (st & ST_OK_MPU) ? "ok" : "-", (st & ST_OK_IRQ) ? "ok" : "-",
        (st & ST_OK_DMA8) ? "ok" : "-", (st & ST_OK_DMA16) ? "ok" : "-", (st & ST_OK_JOY) ? "ok" : "-",
        (st & ST_OK_JBIOS) ? "ok" : "-", (st & ST_OK_PROG) ? "ok" : "-");
    if (st & ST_ERR_SBPORT)  dbg('E', "SB ports %X-%X belong to another driver (a real Sound Blaster's?)", (u32)cfg.sb_port, (u32)cfg.sb_port + 15);
    if (st & ST_ERR_OPLPORT) dbg('E', "OPL ports 388-38B belong to another driver");
    if (st & ST_ERR_MPUPORT) dbg('E', "MPU-401 ports %X-%X belong to another driver", (u32)cfg.mpu_port, (u32)cfg.mpu_port + 1);
    if (st & ST_ERR_IRQ)     dbg('E', "IRQ %u could not be virtualized (another driver owns it)", (u32)cfg.irq);
    if (st & ST_ERR_DMA8)    dbg('E', "DMA %u could not be virtualized", (u32)cfg.dma8);
    if (st & ST_ERR_DMA16)   dbg('E', "DMA %u could not be virtualized", (u32)cfg.dma16);
    if (st & ST_ERR_JOYPORT) dbg('E', "game port %X belongs to another driver", joy_port);
    if (st & ST_ERR_JBIOS)   dbg('E', "INT 15h could not be hooked");
    if (st & ST_ERR_PROG)    dbg('E', "INT 21h could not be hooked (no pad layouts per game)");
    if (st & (ST_ERR_BLOCK | ST_ERR_LOCK | ST_ERR_BUSY)) dbg('E', "attach failed: status %05X", st);
    awe_dbg_attach(st);                         /* AWE line: EMU8000 traps (sbcore.c) */
}

/* commands from the helper (IOCTL_SC55W_DBG_CMD) */
static u32 dbg_cmd(u32 c, char *msg, u32 msgmax)
{
    msg[0] = 0;
    switch (c) {
    case DBGCMD_STATUS:
        if (!dbg_base) fmt(msg, (int)msgmax, "off");
        else fmt(msg, (int)msgmax, "COM%u (%X) %u baud level %u fifo %u sent %u lost %u%s", dbg_port, dbg_base, dbg_baud,
                 dbg_level, dbg_fifo, dbg_sent, dbg_lost, cap_dumping ? " DUMPING" : "");
        return 0;
    case DBGCMD_DUMP:
        if (!dbg_base || dbg_level != 3 || !cap) { fmt(msg, (int)msgmax, "the capture needs /DBG level 3"); return 1; }
        dbg('I', "capture: %u port accesses", cap_head > CAP_N ? CAP_N : cap_head);
        cap_dump = 0; cap_dumping = 1;
        vxd_dbg_arm(2);
        fmt(msg, (int)msgmax, "sending the capture");
        return 0;
    case DBGCMD_HANG:
        if (!dbg_base) { fmt(msg, (int)msgmax, "the hang test needs the link on"); return 1; }
        dbg_raw(1, "VXD", '!', "hang test (SC55W /DBG:HANG): interrupts off, Windows stops now - this must be the last line");
        vxd_hang();
        return 0;
    case DBGCMD_FAULT:
        if (!dbg_base) { fmt(msg, (int)msgmax, "the fault test needs the link on"); return 1; }
        dbg('I', "fault test (SC55W /DBG:FAULT): an invalid opcode (06h) in SC55W.VXD now - the S lines must go on");
        vxd_fault_test();
        fmt(msg, (int)msgmax, "fault test: Windows went on");
        return 0;
    }
    return 3;
}
