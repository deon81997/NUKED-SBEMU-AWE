/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* dbg.h - live debug link over a serial port, shared by SBEMU (sbemu/dbglink.c) and the tools (MENU, MIDPLAY, GAMETEST,
 * SBDIAG). Short text lines go into a RAM ring; polled UART writes drain it (no UART interrupt, no DOS/BIOS calls), a few
 * bytes per call, so it is safe from SBEMU's sound interrupt and port traps. dbg_sync() drains with a time limit (errors,
 * fatal paths: the last line before a freeze gets out). Off = base 0: nothing is touched.
 *
 * Spec (SBEMU /DBG:..., tools /DBG:... or env SBDBG): COM1 | COM2 | COM3 | COM4 | 1..4 | hex base (2F8) [,baud] [,level]
 *   ':' works as well as ',' (DOS batch files split their %1 %2 at commas: SC55 /DBG:COM1:2)
 *   numbers after the port: 1..3 = level, >= 300 = baud (default 115200, level 1). E.g. /DBG:COM1  /DBG:COM2,57600,2
 * Tools: when SBEMU is loaded with /DBG, their lines go through SBEMU (AMIS 0Ah) so the two never mix bytes mid-line;
 * otherwise they open the port themselves.
 * Host side: pack/FILES/SC55/DBGLOG.PY (pyserial, 8N1), cables in DBGLOG.TXT. */
#ifndef NSB_DBG_H
#define NSB_DBG_H
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pc.h>

#define DBG_RING      8192u          /* power of two */
#define DBG_DEFBAUD   115200ul

typedef struct
{
    uint16_t base;                   /* UART I/O base, 0 = off */
    uint8_t  level, fifo;            /* fifo: bytes per THRE (16 on a 16550A, 1 otherwise) */
    uint32_t baud;
    volatile uint32_t head, tail;    /* ring write / read positions (free-running) */
    uint32_t lost, sent;
    char ring[DBG_RING];
} DbgPort;

static inline uint32_t dbg_irqoff(void) { uint32_t f; __asm__ volatile("pushfl; popl %0; cli" : "=r"(f) :: "memory"); return f; }
static inline void dbg_irqon(uint32_t f) { if (f & 0x200) __asm__ volatile("sti" ::: "memory"); }

/* "COM1,115200,2" -> base/baud/level; 0 ok, -1 bad. base from the BIOS data area for COM1..4 (bda = BDA word or 0) */
static inline int dbg_parse(const char* s, int* com, uint16_t* base, uint32_t* baud, int* level)
{
    unsigned long v; char* e;
    *com = 0; *base = 0; *baud = DBG_DEFBAUD; *level = 1;
    if (!s || !*s) return -1;
    if (!strncasecmp(s, "COM", 3)) { s += 3; if (*s < '1' || *s > '4') return -1; *com = *s++ - '0'; }
    else
    {
        v = strtoul(s, &e, 16); if (e == s) return -1;
        if (v >= 1 && v <= 4 && e == s + 1) *com = (int)v; else if (v >= 0x100 && v <= 0xFFF8) *base = (uint16_t)v; else return -1;
        s = e;
    }
    while (*s == ',' || *s == ':')
    {
        v = strtoul(++s, &e, 10); if (e == s) return -1; s = e;
        if (v >= 1 && v <= 3) *level = (int)v; else if (v >= 300 && v <= 115200) *baud = v; else return -1;
    }
    return *s ? -1 : 0;
}

/* program the UART: 8N1, divisor, FIFO on, DTR+RTS (OUT2 off: no UART interrupts). 0 ok, -1 no UART there */
static inline int dbg_uart_init(DbgPort* d, uint16_t base, uint32_t baud, int level)
{
    uint16_t div = (uint16_t)(115200ul / (baud ? baud : DBG_DEFBAUD)); uint8_t iir;
    if (!div) div = 1;
    outportb(base + 7, 0x5A); if (inportb(base + 7) != 0x5A) { outportb(base + 3, 0x03); if (inportb(base + 3) != 0x03) return -1; }
    outportb(base + 1, 0);                       /* no UART interrupts */
    outportb(base + 3, 0x80); outportb(base + 0, div & 0xFF); outportb(base + 1, div >> 8);
    outportb(base + 3, 0x03);
    outportb(base + 2, 0x07);                    /* FIFO on + clear */
    iir = inportb(base + 2);
    outportb(base + 4, 0x03);
    d->fifo = ((iir & 0xC0) == 0xC0) ? 16 : 1;
    d->base = base; d->baud = 115200ul / div; d->level = (uint8_t)level;
    d->head = d->tail = 0; d->lost = d->sent = 0;
    return 0;
}

/* move bytes from the ring to the UART while it takes them without waiting: at most one FIFO load (16 bytes) */
static inline int dbg_pump(DbgPort* d)
{
    int n = 0;
    if (!d->base || d->head == d->tail) return 0;
    if (!(inportb(d->base + 5) & 0x20)) return 0;
    while (n < d->fifo && d->tail != d->head) { outportb(d->base, (uint8_t)d->ring[d->tail & (DBG_RING - 1)]); d->tail++; n++; }
    d->sent += (uint32_t)n;
    return n;
}

/* wait until the ring is empty, at most about 2x the time the bytes need on the wire (+ 50 ms) */
static inline void dbg_sync(DbgPort* d)
{
    uint32_t left, spin;
    if (!d->base) return;
    left = d->head - d->tail;
    spin = (left + 16) * (20000000u / (d->baud ? d->baud : 9600)) / 16 + 500000u;   /* LSR reads (~1 us each) */
    while (d->head != d->tail && spin--) dbg_pump(d);
    spin = 200000u; while (spin-- && !(inportb(d->base + 5) & 0x40)) {}           /* last byte off the shift register */
}

/* append raw bytes; drops the whole line (counts it) if the ring has no room. Interrupt-safe. */
static inline int dbg_put(DbgPort* d, const char* s, int len)
{
    uint32_t f, h; int i;
    if (!d->base || len <= 0) return 0;
    f = dbg_irqoff();
    h = d->head;
    if (DBG_RING - (h - d->tail) < (uint32_t)len + 1) { d->lost++; dbg_irqon(f); return 0; }
    for (i = 0; i < len; i++) d->ring[(h + (uint32_t)i) & (DBG_RING - 1)] = s[i];
    d->head = h + (uint32_t)len;
    dbg_irqon(f);
    return len;
}

/* one line: "<tag> <text>\r\n" (the text gets cut at 200 chars). sync: dbg_sync() afterwards */
static inline void dbg_vline(DbgPort* d, int sync, const char* tag, const char* fmt, va_list ap)
{
    char b[240]; int n;
    if (!d->base) return;
    n = snprintf(b, 40, "%s ", tag);
    n += vsnprintf(b + n, 200, fmt, ap);
    if (n > 236) n = 236;
    while (n && (b[n - 1] == '\n' || b[n - 1] == '\r')) n--;
    b[n++] = '\r'; b[n++] = '\n';
    dbg_put(d, b, n);
    if (sync) dbg_sync(d); else dbg_pump(d);
}

#ifndef DBG_NO_TOOL
/* ---- tools: one DbgPort per program, lines tagged with the tool's name. dbg_tool_init() looks for /DBG:... in argv
 * (removed from argv), else env SBDBG; with SBEMU loaded and its link on, lines go through SBEMU (AMIS 0Ah ECX=1). */
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/movedata.h>
#include <stdlib.h>
static DbgPort g_dbgp;
static int g_dbg_amis;                  /* SBEMU's AMIS id when lines go through SBEMU, else 0 */
static int g_dbg_level;                 /* 0 = off */
static char g_dbg_tag[12];

static inline int dbg_sbemu_amis(void)
{
    static const char sig[] = "Crazii  SBEMU   ";
    int id;
    for (id = 1; id <= 0xFF; ++id)
    {
        __dpmi_regs r; char got[16];
        memset(&r, 0, sizeof r); r.h.ah = (unsigned char)id; r.h.al = 0;
        if (__dpmi_int(0x2D, &r) != 0) return 0;
        if (r.h.al != 0xFF) continue;
        dosmemget((unsigned long)r.x.dx * 16 + r.x.di, 16, got);
        if (!memcmp(got, sig, 16)) return id;
    }
    return 0;
}
/* SBEMU's link: level (0 = off or SBEMU too old) and "DBG=COM1 BAUD=... LVL=..." into out */
static inline int dbg_sbemu_query(int id, char* out, int size)
{
    __dpmi_regs r;
    if (out && size) out[0] = 0;
    if (!id) return 0;
    memset(&r, 0, sizeof r); r.h.ah = (unsigned char)id; r.h.al = 0x0A; r.d.ecx = 0; r.d.ebx = __tb; r.d.edx = 120;
    { char z = 0; dosmemput(&z, 1, __tb); }
    if (__dpmi_int(0x2D, &r) != 0 || r.h.al != 0 || (r.x.flags & 1)) return 0;
    if (out && size) { dosmemget(__tb, size < 120 ? size : 120, out); out[size - 1] = 0; }
    return (int)(r.d.edx & 0xFF);
}
/* AMIS 0Ah requests other than a line: 2 = send the level-3 capture now, 3 = forced hang test, 4 = set level (arg) */
static inline int dbg_sbemu_cmd(int id, int fn, int arg)
{
    __dpmi_regs r;
    if (!id) return -1;
    memset(&r, 0, sizeof r); r.h.ah = (unsigned char)id; r.h.al = 0x0A; r.d.ecx = (unsigned long)fn; r.d.edx = (unsigned long)arg;
    if (__dpmi_int(0x2D, &r) != 0 || r.h.al != 0 || (r.x.flags & 1)) return -1;
    return (int)r.d.edx;
}

static void dbg_done(void);
static __attribute__((unused, format(printf, 1, 2))) void dbg_log(const char* fmt, ...);
static inline int dbg_tool_init(int* argc, char** argv, const char* tag)
{
    const char* spec = NULL; int i, com, lvl; uint16_t base; uint32_t baud;
    strncpy(g_dbg_tag, tag, sizeof g_dbg_tag - 1);
    for (i = 1; argc && i < *argc; i++)
        if (!strncasecmp(argv[i], "/DBG:", 5) || !strncasecmp(argv[i], "-DBG:", 5))
        {
            spec = argv[i] + 5;
            memmove(&argv[i], &argv[i + 1], (size_t)(*argc - i) * sizeof(char*)); (*argc)--; break;
        }
    if (!spec) spec = getenv("SBDBG");
    g_dbg_amis = dbg_sbemu_amis();
    if (g_dbg_amis && (lvl = dbg_sbemu_query(g_dbg_amis, NULL, 0)) > 0 && (!spec || !strcasecmp(spec, "SBEMU")))
        { g_dbg_level = lvl; dbg_log("started"); return lvl; }
    g_dbg_amis = 0;
    if (!spec || !*spec || !strcasecmp(spec, "OFF") || !strcasecmp(spec, "SBEMU")) return 0;
    if (dbg_parse(spec, &com, &base, &baud, &lvl)) { fprintf(stderr, "Bad /DBG:%s (COM1|COM2[,baud][,level])\n", spec); return 0; }
    if (com) base = _farpeekw(_dos_ds, 0x400 + (com - 1) * 2);
    if (!base || dbg_uart_init(&g_dbgp, base, baud, lvl)) { fprintf(stderr, "/DBG: no serial port at COM%d\n", com); return 0; }
    g_dbg_level = lvl;
    atexit(dbg_done);
    dbg_log("started");
    return lvl;
}
static inline void dbg_tvlog(int sync, const char* fmt, va_list ap)
{
    if (!g_dbg_level) return;
    if (g_dbg_amis)
    {
        char b[220]; int n; __dpmi_regs r;
        n = snprintf(b, 16, "%s ", g_dbg_tag);
        n += vsnprintf(b + n, sizeof b - (size_t)n, fmt, ap);
        if (n > (int)sizeof b - 1) n = sizeof b - 1;
        dosmemput(b, (size_t)n + 1, __tb);
        memset(&r, 0, sizeof r); r.h.ah = (unsigned char)g_dbg_amis; r.h.al = 0x0A; r.d.ecx = 1; r.d.ebx = __tb; r.d.edx = (unsigned long)sync;
        __dpmi_int(0x2D, &r);
        return;
    }
    {   /* own port: "<s.ms> TAG text", seconds since midnight from the BIOS clock (55 ms steps) */
        char pre[32]; unsigned long ms = (unsigned long)_farpeekl(_dos_ds, 0x46C) * 10000ul / 182ul;
        snprintf(pre, sizeof pre, "%lu.%03lu %s", ms / 1000, ms % 1000, g_dbg_tag);
        dbg_vline(&g_dbgp, sync, pre, fmt, ap);
    }
}
static __attribute__((unused, format(printf, 1, 2))) void dbg_log(const char* fmt, ...)
{ va_list ap; if (!g_dbg_level) return; va_start(ap, fmt); dbg_tvlog(0, fmt, ap); va_end(ap); }
static __attribute__((unused, format(printf, 1, 2))) void dbg_err(const char* fmt, ...)   /* sent before returning */
{ va_list ap; if (!g_dbg_level) return; va_start(ap, fmt); dbg_tvlog(1, fmt, ap); va_end(ap); }
static __attribute__((unused)) void dbg_poll(void) { if (g_dbg_level && !g_dbg_amis) dbg_pump(&g_dbgp); }
static void dbg_done(void) { if (g_dbg_level && !g_dbg_amis) dbg_sync(&g_dbgp); }
#define DBG_LOG(...) do { if (g_dbg_level) dbg_log(__VA_ARGS__); } while (0)
#endif
#endif
