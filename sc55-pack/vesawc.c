/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* vesawc.c - SBEMU /VWC:1: write-combining (WC) for the VESA linear frame buffer through one free variable MTRR.
 *
 * Why: a game in a VESA LFB mode writes its frames to uncached video memory; with WC the CPU gathers the writes into
 * 32/64-byte bursts, so drawing the screen takes much less CPU time and more is left for SBEMU + the SC-55
 * (deon81997/3dfx-dos-emulator- vcore/DOSPERF3 cut its screen present time a lot on the Dell D810 this way).
 *
 * The MTRR work is tools/wcset.h (WCSET.EXE, MENU, MIDPLAY): CPUID/MTRRcap checks, a free variable MTRR only,
 * nothing that overlaps an MTRR already set, the SDM 11.11.7.2 write sequence. MSRs need ring 0; SBEMU is a
 * ring-3 DPMI client, so a tiny real-mode routine does it through DPMI 0301h:
 *   no EMM (HIMEM + HDPMI32I, SC55.BAT)   real mode: works
 *   JEMM / JEMMEX (SC55JEM.BAT)           V86 mode, JEMM emulates RDMSR/WRMSR/MOV CRx/WBINVD: works
 *   EMM386 / QEMM / other V86 monitors    they fault or reboot on those instructions: refused (only SMSW, harmless,
 *                                         is run to see V86 mode), message, SBEMU goes on without WC
 *   Windows / OS/2 / NT DOS box           refused (Windows' display driver owns the MTRRs)
 * (The 3dfx OVL ran under DOS/4GW at ring 0 and used RDMSR/WRMSR directly; a ring-3 client cannot, hence the stub.)
 *
 * Never A0000-BFFFF: only the linear frame buffer above 1 MB is ever made WC (planar VGA / Mode X read video memory
 * back through the latches; the legacy window is a fixed-range MTRR anyway, which is never touched).
 * Frame buffer: VBE 4F03 current mode or the first LFB mode (4F01 PhysBasePtr), checked against the display
 * adapter's PCI memory BAR (read only; the fallback when VBE gives nothing sizes the BAR); size = VBE total memory.
 * Lifetime: SBEMU stays resident (it cannot be unloaded), so WC stays until reboot or "SBEMU /VWC:0" (a re-run takes
 * the MTRR SBEMU set away again). If SBEMU exits without going resident, the MTRR is restored at once (atexit).
 * Cost: none at run time (nothing here runs after start-up); /VWC:0 (default) does not even run CPUID. */
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tools/wcset.h"
#include "vesawc.h"
#include <stdarg.h>
#include <dos.h>

static void (*vwc_logf)(const char* fmt, ...);
#define VLOG(...) do { if(vwc_logf) vwc_logf(__VA_ARGS__); } while(0)

/* ---- memory manager name, for the messages ---- */
static int vwc_has_dev(const char* name)
{
    __dpmi_regs r; unsigned short h;
    dosmemput(name, strlen(name) + 1, __tb);
    memset(&r, 0, sizeof r); r.x.ax = 0x3D00; r.x.ds = __tb >> 4; r.x.dx = __tb & 15; __dpmi_int(0x21, &r);
    if(r.x.flags & 1) return 0;
    h = r.x.ax; memset(&r, 0, sizeof r); r.x.ax = 0x3E00; r.x.bx = h; __dpmi_int(0x21, &r);
    return 1;
}
static int vwc_is_qemm(void)
{
    __dpmi_regs r;
    memset(&r, 0, sizeof r); r.x.ax = 0xD200; r.x.bx = 0x5145; r.x.cx = 0x4D4D; r.x.dx = 0x3432; __dpmi_int(0x2F, &r);
    if(r.h.al == 0xFF && r.x.bx == 0x4D45 && r.x.cx == 0x4D44 && r.x.dx == 0x5652) return 1;
    return vwc_has_dev("QEMM386$");
}
static const char* vwc_mm(void)
{
    if(WC.win) return "Windows";
    if(!WC.v86) return "no EMM (real mode)";
    if(WC.jemm) return "JEMM";
    if(vwc_is_qemm()) return "QEMM";
    if(vwc_has_dev("EMMXXXX0") || vwc_has_dev("EMMQXXX0")) return "EMM386";
    return "a V86 monitor";
}
static int vwc_cpl(void) { unsigned short cs; __asm__("movw %%cs, %0" : "=r"(cs)); return cs & 3; }

/* ---- PCI BIOS (INT 1Ah B1xxh), so SBDIAG can use this file too ---- */
static unsigned long vwc_pci_rd(int bdf, int reg)
{
    __dpmi_regs r; memset(&r, 0, sizeof r);
    r.x.ax = 0xB10A; r.x.bx = (unsigned short)bdf; r.x.di = (unsigned short)reg; __dpmi_int(0x1A, &r);
    return (r.x.flags & 1) || r.h.ah ? 0xFFFFFFFFUL : r.d.ecx;
}
static void vwc_pci_wr(int bdf, int reg, unsigned long v)
{
    __dpmi_regs r; memset(&r, 0, sizeof r);
    r.x.ax = 0xB10D; r.x.bx = (unsigned short)bdf; r.x.di = (unsigned short)reg; r.d.ecx = v; __dpmi_int(0x1A, &r);
}
static int vwc_pci_present(void)
{
    __dpmi_regs r; memset(&r, 0, sizeof r); r.x.ax = 0xB101; __dpmi_int(0x1A, &r);
    return !(r.x.flags & 1) && r.h.ah == 0 && r.d.edx == 0x20494350UL;   /* "PCI " */
}
/* the display adapter's memory BAR that holds addr (addr 0: its largest one). base/size out; 1 found.
 * sizeit 0 (VBE already gave address + memory size): read only, the BAR nearest below addr, size 0 = not known.
 * sizeit 1 (fallback, VBE gave nothing): the BAR is sized (its memory decode is off for those few config writes). */
static int vwc_pci_fb(unsigned long addr, int sizeit, unsigned long* base, unsigned long* size, int* bdf_out)
{
    int idx;
    if(!vwc_pci_present()) return 0;
    for(idx = 0; idx < 8; idx++)
    {
        __dpmi_regs r; int bdf, b, best = -1; unsigned long bb = 0, bs = 0;
        memset(&r, 0, sizeof r); r.x.ax = 0xB103; r.d.ecx = 0x030000UL; r.x.si = (unsigned short)idx; __dpmi_int(0x1A, &r);
        if((r.x.flags & 1) || r.h.ah) break;                         /* no more VGA-compatible controllers */
        bdf = r.x.bx;
        for(b = 0; b < 6; b++)
        {
            int reg = 0x10 + 4 * b;
            unsigned long v = vwc_pci_rd(bdf, reg), sz, cmd;
            if(v == 0xFFFFFFFFUL || (v & 1)) continue;                 /* I/O BAR */
            if(((v >> 1) & 3) == 2) { b++; if(vwc_pci_rd(bdf, reg + 4)) continue; } /* 64-bit BAR above 4 GB: skip */
            if(!(v & ~0xFUL)) continue;
            if(!sizeit)
            { v &= ~0xFUL; if(addr && v <= addr && v > bb) { bb = v; best = b; } continue; }
            /* size it: memory decode off for the few microseconds the BAR reads back its mask (as a BIOS does) */
            cmd = vwc_pci_rd(bdf, 0x04);
            disable();
            vwc_pci_wr(bdf, 0x04, cmd & ~2UL);
            vwc_pci_wr(bdf, reg, 0xFFFFFFFFUL);
            sz = vwc_pci_rd(bdf, reg);
            vwc_pci_wr(bdf, reg, v);
            vwc_pci_wr(bdf, 0x04, cmd);
            enable();
            sz = ~(sz & ~0xFUL) + 1;
            if(!sz || (sz & (sz - 1))) continue;
            v &= ~0xFUL;
            if(addr) { if(addr >= v && addr - v < sz) { *base = v; *size = sz; *bdf_out = bdf; return 1; } continue; }
            if(sz > bs) { best = b; bb = v; bs = sz; }
        }
        if(best >= 0 && (!sizeit || !addr)) { *base = bb; *size = sizeit ? bs : 0; *bdf_out = bdf; return 1; }
    }
    return 0;
}

/* ---- the frame buffer and its WC range ---- */
static unsigned long vwc_bar_base, vwc_bar_size; static int vwc_bar_bdf = -1, vwc_lfb_from;  /* 1 current mode, 2 mode list, 3 PCI */
static int vwc_find(void)
{
    __dpmi_regs r; unsigned char mi[256];
    WC.lfb = 0; WC.vram = 0;
    memset(&r, 0, sizeof r); r.x.ax = 0x4F03; __dpmi_int(0x10, &r);   /* active mode first, if it is a VBE LFB mode */
    if(r.x.ax == 0x004F && (r.x.bx & 0x1FF) >= 0x100)
    {
        unsigned m = r.x.bx & 0x1FF;
        memset(&r, 0, sizeof r); r.x.ax = 0x4F01; r.x.cx = m; r.x.es = __tb >> 4; r.x.di = __tb & 15; __dpmi_int(0x10, &r);
        dosmemget(__tb, 256, mi);
        if(r.x.ax == 0x004F && (mi[0] & 0x81) == 0x81)
        { WC.lfb = mi[0x28] | (mi[0x29] << 8) | ((unsigned long)mi[0x2A] << 16) | ((unsigned long)mi[0x2B] << 24); vwc_lfb_from = 1; }
    }
    wc_find_lfb();                                                    /* VBE 2.0 total memory + first LFB mode */
    if(WC.lfb && !vwc_lfb_from) vwc_lfb_from = 2;
    vwc_bar_bdf = -1;
    if(vwc_pci_fb(WC.lfb, !(WC.lfb && WC.vram), &vwc_bar_base, &vwc_bar_size, &vwc_bar_bdf))
    {
        if(!WC.lfb) { WC.lfb = vwc_bar_base; vwc_lfb_from = 3; }
        if(vwc_bar_size && (!WC.vram || WC.vram > vwc_bar_size - (WC.lfb - vwc_bar_base))) WC.vram = vwc_bar_size - (WC.lfb - vwc_bar_base);
    }
    if(!WC.lfb) return 0;
    if(!wc_range()) return 0;
    if(vwc_bar_bdf >= 0 && vwc_bar_size)                               /* never past the end of the BAR */
        while(WC.r_size > 0x100000 && WC.r_base + WC.r_size > (wc_u64)vwc_bar_base + vwc_bar_size) WC.r_size >>= 1;
    /* hard rule: never anything below 1 MB (A0000-BFFFF: planar VGA / Mode X latches read video memory) */
    if(WC.r_base < 0x100000ULL || WC.r_base + WC.r_size > (1ULL << WC.phys_bits)) return 0;
    return 1;
}
static void vwc_where(char* o, int n)
{
    snprintf(o, n, "LFB %08lX from %s, %lu MB", WC.lfb, vwc_lfb_from == 1 ? "the current VBE mode" : vwc_lfb_from == 2 ? "the VBE mode list" : "the PCI BAR",
             WC.vram >> 20);
    if(vwc_bar_bdf >= 0)
    {
        int l = (int)strlen(o);
        l += snprintf(o + l, n - l, ", display %02X:%02X.%X BAR %08lX", vwc_bar_bdf >> 8, (vwc_bar_bdf >> 3) & 31, vwc_bar_bdf & 7, vwc_bar_base);
        if(vwc_bar_size) snprintf(o + l, n - l, " %lu MB", vwc_bar_size >> 20);
    }
}
static void vwc_dump(char* o, int n)                                  /* the 3dfx PERFLOG "MTRRs:" line */
{
    int i, l = snprintf(o, n, "default %s%s%s", wc_type_name(WC.def_type), WC.def_e ? "" : " (MTRRs off)", WC.fix ? ", fixed on" : "");
    for(i = 0; i < WC.vcnt && l < n - 40; i++)
        if(wc_valid(i)) l += snprintf(o + l, n - l, "; %d: %s %08llX %llu MB", i, wc_type_name((int)(WC.m[i].base & 0xFF)), wc_mbase(i), wc_msize(i) >> 20);
        else l += snprintf(o + l, n - l, "; %d: free", i);
}

static const char* vwc_why(void)
{
    static char w[160];
    if(WC.v86 && !WC.jemm && !WC.win)
    { snprintf(w, sizeof w, "%s runs DOS in V86 mode and does not let programs write CPU MTRRs; use SC55 (HIMEM only) or SC55JEM (JEMM)", vwc_mm()); return w; }
    return WC.why;
}

/* SBEMU already resident? (AMIS signature, before this SBEMU installs its own handler) */
static int vwc_resident(void)
{
    static const char sig[] = "Crazii  SBEMU   ";
    int i;
    for(i = 0; i <= 0xFF; i++)
    {
        __dpmi_regs r; char s[16];
        memset(&r, 0, sizeof r); r.h.ah = (unsigned char)i; r.h.al = 0; __dpmi_int(0x2D, &r);
        if(r.h.al != 0xFF) continue;
        dosmemget(((unsigned long)r.x.dx << 4) + r.x.di, 16, s);
        if(!memcmp(s, sig, 16)) return 1;
    }
    return 0;
}

static void vwc_atexit(void)                                          /* SBEMU quit without going resident */
{
    if(WC.changed < 0) return;
    wc_restore();
    printf("Video write-combining: undone (SBEMU did not stay resident)\n");
}

void VESAWC_Init(int value, int given, void (*log)(const char* fmt, ...))
{
    char msg[200], w[160], d[400]; int resident;
    vwc_logf = log;
    if(!value && !given) { VLOG("Video write-combining (/VWC): off (default; /VWC:1 turns it on)\n"); return; }
    resident = vwc_resident();
    if(!wc_probe()) { snprintf(msg, sizeof msg, "not used - %s", vwc_why()); goto out; }
    if(!vwc_find()) { snprintf(msg, sizeof msg, "not used - no VESA 2.0 linear frame buffer above 1 MB found"); goto out; }
    vwc_where(w, sizeof w); vwc_dump(d, sizeof d);
    VLOG("Video write-combining: CPU %s family %d model %d, %d-bit addresses, %d variable MTRRs, CPL %d, %s\n",
         WC.vendor, WC.family, WC.model, WC.phys_bits, WC.vcnt, vwc_cpl(), vwc_mm());
    VLOG("Video write-combining: %s; range %08llX %llu MB\n", w, WC.r_base, WC.r_size >> 20);
    VLOG("Video write-combining: MTRRs before: %s\n", d);
    if(value)
    {
        char m[160];
        int on = wc_enable(resident, m, sizeof m);                   /* first run: undone if SBEMU does not stay */
        if(on && !resident && WC.changed >= 0) atexit(vwc_atexit);
        snprintf(msg, sizeof msg, "%s%s", on ? "" : "not used - ", m + (!on && !strncmp(m, "off - ", 6) ? 6 : 0));
        if(on && WC.changed >= 0) { char *p = strstr(msg, ", undone at exit"); if(p) strcpy(p, ", until reboot or SBEMU /VWC:0)"); }
        if(on && resident) { char *p = strstr(msg, ", kept"); if(p) strcpy(p, ", until reboot or SBEMU /VWC:0)"); }
    }
    else
    {
        char m[160];
        wc_disable(m, sizeof m);
        snprintf(msg, sizeof msg, "/VWC:0: %s", m);
    }
    wc_read_mtrrs(); vwc_dump(d, sizeof d);
    VLOG("Video write-combining: MTRRs after: %s\n", d);
out:
    VLOG("Video write-combining (/VWC:%d, %s): %s\n", value, resident ? "SBEMU already resident" : "start", msg);
    printf("Video write-combining: %s\n", msg);
}

int VESAWC_Status(char* out, int size)
{
    int i, st; char w[160];
    if(!wc_probe()) { snprintf(out, size, "unavailable - %s", vwc_why()); return -1; }
    if(!vwc_find()) { snprintf(out, size, "unavailable - no VESA 2.0 linear frame buffer above 1 MB found (%s)", vwc_mm()); return -1; }
    vwc_where(w, sizeof w);
    st = wc_check(&i);
    if(st == 1) { snprintf(out, size, "on (MTRR %d = WC %08llX %llu MB; %s; %s)", i, wc_mbase(i), wc_msize(i) >> 20, w, vwc_mm()); return 1; }
    if(st < 0) { snprintf(out, size, "unavailable - MTRR %d (%s %08llX %llu MB) already covers the frame buffer (%s)", i, wc_type_name((int)(WC.m[i].base & 0xFF)), wc_mbase(i), wc_msize(i) >> 20, w); return -1; }
    if(i < 0) { snprintf(out, size, "unavailable - all %d variable MTRRs are in use (%s)", WC.vcnt, w); return -1; }
    snprintf(out, size, "off (possible: SBEMU /VWC:1 would use free MTRR %d; %s; %s)", i, w, vwc_mm());
    return 0;
}
