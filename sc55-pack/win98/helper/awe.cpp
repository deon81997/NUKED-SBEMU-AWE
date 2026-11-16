/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
// awe.cpp - AWE32 (EMU8000) for DOS boxes, see awe.h
#include "awe.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
extern "C" {
#include "../../awe/synth/emu8k.h"
}

static SC55W_AWE* B;
static size_t B_bytes;
static AweInfo info;
static Options built;               // options the block was built for
static std::string built_romdir;
static CRITICAL_SECTION cs;
static bool cs_init;
static HANDLE ev_w32, th;
static volatile LONG th_stop;
static const uint16_t woff[6] = { 0x000, 0x002, 0x400, 0x402, 0x800, 0x802 };   // emu8k_io_write offsets

static bool is_awe_rom(const std::string& p)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &a) || (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
    if (a.nFileSizeHigh || (a.nFileSizeLow != 1048576 && a.nFileSizeLow != 1048578)) return false;
    FILE* f = fopen(p.c_str(), "rb");
    unsigned char h[12];
    bool ok = false;
    if (!f) return false;
    if (fread(h, 1, 12, f) == 12) ok = !memcmp(h + 4, "M1MG", 4) || !memcmp(h + 6, "M1MG", 4);   // as DOS vawe.c
    fclose(f);
    return ok;
}

static std::string find_rom(const std::string& dir)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    std::string r;
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        if (fd.cFileName[0] == '.') continue;
        std::string p = dir + "\\" + fd.cFileName;
        if (is_awe_rom(p)) { r = p; break; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return r;
}

void awe_lock() { if (cs_init) EnterCriticalSection(&cs); }
void awe_unlock() { if (cs_init) LeaveCriticalSection(&cs); }
SC55W_AWE* awe_block() { return info.on ? B : NULL; }
const AweInfo& awe_info() { return info; }

void awe_apply(const SC55W_AWEEV& e)
{
    if (e.idx < 6) emu8k_io_write(woff[e.idx], e.val);
}

void awe_render(int16_t* out, int frames, int rate) { emu8k_render(out, frames, rate); }

void awe_snapshot()
{
    if (!B) return;
    for (int ch = 0; ch < 32; ch++) {
        B->cpf[ch] = emu8k_read(EMU8K_CPF, ch);
        B->cvcf[ch] = emu8k_read(EMU8K_CVCF, ch);
        B->ccca[ch] = emu8k_read(EMU8K_CCCA, ch);
    }
    MemoryBarrier();
    B->snap_ev = B->ev_tail;
    B->snap_seq++;
}


// If the render thread is not draining the ring (the synth still starting, or starved), apply the oldest
// writes at once before the ring fills up - a lost write would leave the EMU8000 state wrong for good.
static DWORD WINAPI awe_thread(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    while (!th_stop) {
        WaitForSingleObject(ev_w32, 50);
        if (th_stop) break;
        if (B->ev_head - B->ev_tail > AWERING_SIZE / 2) {
            awe_lock();
            while (B->ev_head - B->ev_tail > AWERING_SIZE / 4) {
                awe_apply(B->ev[B->ev_tail & (AWERING_SIZE - 1)]);
                B->ev_tail++;
            }
            B->pumped++;
            awe_unlock();
        }
    }
    return 0;
}

static void stop_thread()
{
    if (!th) return;
    InterlockedExchange(&th_stop, 1);
    SetEvent(ev_w32);
    WaitForSingleObject(th, 2000);
    CloseHandle(th);
    th = NULL;
}

static void free_block()
{
    stop_thread();
    if (info.on) { awe_lock(); emu8k_close(); awe_unlock(); }
    if (B) VirtualFree(B, 0, MEM_RELEASE);
    B = NULL;
    B_bytes = 0;
    info = AweInfo();
}

void awe_shutdown() { free_block(); built = Options(); }

const AweInfo& awe_prepare(const Options& o, const std::string& romdir, SC55W_SHARED* sh)
{
    if (!cs_init) { InitializeCriticalSection(&cs); cs_init = true; }
    if (!ev_w32) ev_w32 = CreateEventA(NULL, FALSE, FALSE, NULL);
    stop_thread();
    bool same = B && o.awe == built.awe && o.aweram == built.aweram && o.awerom == built.awerom && romdir == built_romdir;
    if (!o.awe) free_block();
    else if (!same) {
        free_block();
        const size_t pg = 4096;
        size_t rom_off = (sizeof(SC55W_AWE) + pg - 1) & ~(pg - 1);
        size_t ram_off = (rom_off + emu8k_rom_words() * 2 + pg - 1) & ~(pg - 1);
        size_t total = ram_off + (size_t)emu8k_ram_words(o.aweram) * 2;
        B = (SC55W_AWE*)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        info.port = o.awe; info.ram_kb = o.aweram;
        if (!B) { info.error = "not enough memory for the AWE32 sample RAM"; }
        else {
            B_bytes = total;
            B->magic = SC55W_AWE_MAGIC;
            B->size = (u32)total;
            B->rom_off = (u32)rom_off;
            B->ram_off = (u32)ram_off;
            B->ram_words = (u32)o.aweram * 512u;
            B->ram_alloc = o.aweram ? (u32)(((o.aweram * 512u) + 0xFFFF) >> 16) * 0x10000u : 0;
            uint16_t* rom = (uint16_t*)((char*)B + rom_off);
            uint16_t* ram = o.aweram ? (uint16_t*)((char*)B + ram_off) : NULL;
            awe_lock();
            int r = emu8k_init_mem(o.aweram, rom, ram);
            awe_unlock();
            if (r != 0) { info.error = "EMU8000 engine did not start"; VirtualFree(B, 0, MEM_RELEASE); B = NULL; }
            else {
                // ROM: /AWEROM file or folder, else the ROM folder, then AWE (as DOS SBEMU), by content
                std::string p;
                if (!o.awerom.empty()) {
                    DWORD at = GetFileAttributesA(o.awerom.c_str());
                    p = (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) ? find_rom(o.awerom) : o.awerom;
                }
                if (p.empty()) p = find_rom(romdir);
                if (p.empty()) {
                    std::string up = romdir;
                    size_t s = up.find_last_of("\\/");
                    if (s != std::string::npos) p = find_rom(up.substr(0, s) + "\\AWE");
                }
                if (!p.empty() && emu8k_load_rom(p.c_str()) == 0) info.rom = p;
                info.on = true;
                built = o; built_romdir = romdir;
            }
        }
    }
    if (B && info.on) {
        B->ev_head = B->ev_tail = 0;
        B->snap_ev = 0;
        awe_snapshot();
        sh->awe_port = (u32)info.port;
        sh->awe_addr = (u32)(uintptr_t)B;
        sh->awe_size = (u32)B_bytes;
    } else {
        sh->awe_port = sh->awe_addr = sh->awe_size = 0;
    }
    return info;
}

void awe_attached(unsigned long st)
{
    if (!info.on || !B) return;
    if (!(st & ST_OK_AWE)) { info.error = (st & ST_ERR_AWEPORT) ? "EMU8000 ports in use" : "SC55W.VXD did not take the AWE32 block"; return; }
    InterlockedExchange(&th_stop, 0);
    DWORD tid;                     // Windows 98: CreateThread fails with a NULL thread id
    th = CreateThread(NULL, 0, awe_thread, NULL, 0, &tid);
}
