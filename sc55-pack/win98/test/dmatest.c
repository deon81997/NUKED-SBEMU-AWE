/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* dmatest.c - host test of vxd/dma98.c (awe.7): the timer never gets a raw VDMAD address */
#include <stdio.h>
#include <string.h>
typedef unsigned int u32;
static u32 hl = 0x81400000u, map_calls, map_ret = 0xC2000000u, locks, unlocks, lock_ok = 1, fails;
u32 vxd_vm_high(u32 vm) { return vm ? hl : 0; }
u32 vxd_map_phys(u32 p, u32 n) { map_calls++; return (p == 0 && n == 0x1000000u) ? map_ret : 0; }
u32 vxd_lin_lock(u32 pg, u32 n) { (void)pg; if (!lock_ok) return 0; locks += n; return 1; }
u32 vxd_lin_unlock(u32 pg, u32 n) { (void)pg; unlocks += n; return 1; }
#include "../vxd/dma98.c"
#define check(c, m) do { if (!(c)) { printf("FAIL %s (line %d)\n", m, __LINE__); fails++; } } while (0)
int main(void)
{
    u32 vm = 0xC1000000u, k;
    /* high linear (translation on): locked, read in place */
    k = dma_prove(0, vm, hl + 0x16D60, 8192);
    check(k == (DMA_LOCKED | DMA_NEW), "high linear locked");
    check(locks == 3, "8192 bytes at 6D60 = 3 pages");
    check(dma_at(0, hl + 0x16D60, 4096) == hl + 0x16D60, "read in place");
    check(dma_at(0, hl + 0x16D60 + 8191, 1) == hl + 0x16D60 + 8191, "last byte");
    check(dma_at(0, hl + 0x16D60 + 8191, 2) == 0, "past the end");
    check(dma_at(0, hl + 0x16D60 - 1, 1) == 0, "before the start");
    check(dma_prove(0, vm, hl + 0x16D60, 8192) == DMA_LOCKED, "same buffer: no relock");
    check(locks == 3, "no double lock");
    /* lead #1: physical address (VDS lock) -> alias of 0..16 MB, made once */
    k = dma_prove(0, vm, 0x16D60, 8192);
    check(k == (DMA_PHYS | DMA_NEW), "physical");
    check(unlocks == 3, "old buffer unlocked");
    check(dma_at(0, 0x16D60, 16) == map_ret + 0x16D60, "physical through the alias");
    check(dma_at(0, 0x16D60 + 8192, 1) == 0, "physical past the end");
    dma_prove(0, vm, 0x17D60, 4096);
    check(map_calls == 1, "alias made once");
    check(dma_at(0, 0x17D60, 4096) == map_ret + 0x17D60, "reprogram mid-transfer (lead #2)");
    check(dma_at(0, 0x16D60, 1) == 0, "old range dropped");
    /* above 16 MB and not the VM's window: never read */
    check(dma_prove(0, vm, 0x01216D60, 8192) == DMA_NEW, "above 16 MB unproven");
    check(dma_at(0, 0x01216D60, 1) == 0, "above 16 MB silence");
    check(dma_prove(0, vm, hl + 0x10F000, 0x2000) == DMA_NEW, "past the V86 window");
    /* lock refused -> silence */
    lock_ok = 0;
    check(dma_prove(1, vm, hl + 0x20000, 0x8000) == DMA_NEW, "lock refused");
    check(dma_at(1, hl + 0x20000, 2) == 0, "lock refused silence");
    lock_ok = 1;
    /* alias refused -> silence, retried next time */
    map_ret = 0; phys_win = 0;
    check(dma_prove(1, vm, 0x30000, 0x1000) == DMA_NEW && dma_at(1, 0x30000, 1) == 0, "alias refused");
    /* box gone: unlock everything */
    map_ret = 0xC2000000u;
    dma_prove(1, vm, hl + 0x40000, 0x2000);
    k = locks - unlocks; dma_unprove(0); dma_unprove(1);
    check(locks == unlocks, "all unlocked when the box goes");
    check(dma_at(1, hl + 0x40000, 1) == 0, "nothing readable after");
    (void)k;
    printf(fails ? "dmatest FAIL (%u)\n" : "dmatest PASS\n", fails);
    return fails != 0;
}
