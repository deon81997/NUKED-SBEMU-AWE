/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* dma98.c - awe.7: the timer reads the DOS program's DMA buffer only through an address
 * proven readable (#included by sbcore.c; host test: test/dmatest.c).
 *
 * User crash (P5PE-VM, Descent main menu): exception 0E in convert() reading the SB DMA buffer
 * from Timer_CB.  VDMAD_Get_Virt_State ESI is the VM's HIGH LINEAR address only while VDMAD
 * translation is on; a program that VDS-locks its buffer (DOS4GW / HMI SOS) turns translation
 * off and ESI is then a PHYSICAL address.  Timer_CB runs in whatever VM is current, so a
 * physical address read as linear hits another VM's memory (loud noise) or nothing (0E).
 *
 * Each time the program arms the channel (VDMAD callback, VM context - VMM calls allowed):
 *   high linear inside the VM's 0..10FFFFh window -> _LinPageLock the pages (present until unlocked);
 *   below 16 MB (what the 8237 can reach)         -> physical: read through one _MapPhysToLinear
 *                                                    alias of 0..16 MB (made once, never freed);
 *   anything else, or the lock fails              -> not proven: the timer feeds silence.
 * The timer (any context) only does arithmetic: dma_at() = alias for [a, a+k) inside the proven
 * range, else 0.  A reprogram with a count past the buffer (lead #2) stays inside what was proven. */

#define DMA_HL_SPAN  0x110000UL    /* V86 address space incl. the HMA */
#define DMA_PHYS_TOP 0x1000000UL   /* 8237 + page register: 24 bits */

enum { DMA_NONE = 0, DMA_LOCKED = 1, DMA_PHYS = 2, DMA_NEW = 0x100 };

static struct {
    u32 kind;                     /* DMA_NONE / DMA_LOCKED / DMA_PHYS */
    u32 raw, len;                 /* proven raw VDMAD range */
    u32 lin;                      /* where raw is readable */
    u32 lpage, lpages;            /* pages we locked (DMA_LOCKED) */
} dreg[2];
static u32 phys_win;              /* alias of physical 0..16 MB, 0 = not made / failed */
static u32 c_dmabad;              /* bytes the timer could not read (fed as silence) */

static void dma_unprove(int idx)
{
    if (dreg[idx].kind == DMA_LOCKED && dreg[idx].lpages) vxd_lin_unlock(dreg[idx].lpage, dreg[idx].lpages);
    dreg[idx].kind = DMA_NONE; dreg[idx].len = 0; dreg[idx].lpages = 0;
}

/* VM context: prove [a, a+n) readable for the timer. Returns the kind, | DMA_NEW if it changed. */
static u32 dma_prove(int idx, u32 vm, u32 a, u32 n)
{
    u32 hl, p0, np;
    if (dreg[idx].kind && dreg[idx].raw == a && dreg[idx].len == n) return dreg[idx].kind;
    dma_unprove(idx);
    if (!n) return DMA_NEW;
    hl = vxd_vm_high(vm);
    if (hl && a - hl < DMA_HL_SPAN && n <= DMA_HL_SPAN - (a - hl)) {
        p0 = a >> 12; np = ((a + n - 1) >> 12) - p0 + 1;
        if (!vxd_lin_lock(p0, np)) return DMA_NEW;
        dreg[idx].lpage = p0; dreg[idx].lpages = np;
        dreg[idx].lin = a; dreg[idx].kind = DMA_LOCKED;
    } else if (a < DMA_PHYS_TOP && n <= DMA_PHYS_TOP - a) {
        if (!phys_win) phys_win = vxd_map_phys(0, DMA_PHYS_TOP);
        if (!phys_win) return DMA_NEW;
        dreg[idx].lin = phys_win + a; dreg[idx].kind = DMA_PHYS;
    } else return DMA_NEW;
    dreg[idx].raw = a; dreg[idx].len = n;
    return dreg[idx].kind | DMA_NEW;
}

/* any context: readable address for k bytes at raw VDMAD address a, 0 = not proven */
static u32 dma_at(int idx, u32 a, u32 k)
{
    u32 off = a - dreg[idx].raw;
    if (!dreg[idx].kind || off >= dreg[idx].len || k > dreg[idx].len - off) return 0;
    return dreg[idx].lin + off;
}
