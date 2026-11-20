/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
// awe.cpp - AWE32 (EMU8000) for DOS boxes, see awe.h
#include "awe.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <vector>
extern "C" {
#include "../../awe/synth/emu8k.h"
#include "../../awe/synth/awemidi.h"
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

static bool m_started;
static std::string m_ini;               // SC55W.INI (bank manager writes at once)
static bool m_dos_awe;                  // /AWE + /AWEMPU:GM|GS|MT32: DOS boxes' MPU-401 -> this synth
static int m_dos_mode = -1;             // bank the DOS box's MPU-401 asked for (-1 = Settings' bank)
static DWORD m_dos_last;
static std::string m_upath[128];        // user bank files as loaded
static std::vector<AwePresetCfg> m_ppath;   // single presets as loaded
static AweMidiCfg mcfg;
static bool want_engine() { return mcfg.on || m_dos_awe; }
static void apply_banks();
static void ram_sync();
void awe_render(int16_t* out, int frames, int rate)
{
    emu8k_render(out, frames, rate);
    if (m_started) awem_advance(frames * 44100 / rate);
}

void awe_fx(int on)
{
    static int cur = -1;
    if (on == cur) return;
    awe_lock(); emu8k_set_fx(on); awe_unlock();
    cur = on;
}

void awe_snapshot()
{
    if (!B) return;
    for (int ch = 0; ch < 32; ch++) {
        B->cpf[ch] = emu8k_read(EMU8K_CPF, ch);
        B->cvcf[ch] = emu8k_read(EMU8K_CVCF, ch);
        B->ccca[ch] = emu8k_read(EMU8K_CCCA, ch);
        B->vtft[ch] = emu8k_read(EMU8K_VTFT, ch);   // beta.131-awe.4: live VTFT (HMI picks voices by it)
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

static void midi_engine_down();
static void midi_engine_up();
static void free_block()
{
    stop_thread();
    if (info.on) { awe_lock(); midi_engine_down(); emu8k_close(); awe_unlock(); }
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
    m_dos_awe = o.awe && o.awempu >= AWEMPU_GM;
    const bool need = o.awe || awe_midi_cfg().on;     // the DOS boxes' AWE32 and/or the AWE32 MIDI Synth
    bool same = B && o.aweram == built.aweram && o.awerom == built.awerom && romdir == built_romdir;
    if (!need) free_block();
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
                awe_lock(); midi_engine_up(); awe_unlock();
            }
        }
    }
    info.port = o.awe;
    if (B && info.on) {          // /AWEMPU:GM etc. may need the synth started (or no longer)
        awe_lock();
        if (want_engine() && !m_started) apply_banks();
        else if (!want_engine() && m_started) { awem_release(); m_started = false; }
        awe_unlock();
    }
    if (B && info.on && o.awe) {
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
    if (!info.on || !B || !info.port) return;
    if (!(st & ST_OK_AWE)) { info.error = (st & ST_ERR_AWEPORT) ? "EMU8000 ports in use" : "SC55W.VXD did not take the AWE32 block"; return; }
    InterlockedExchange(&th_stop, 0);
    DWORD tid;                     // Windows 98: CreateThread fails with a NULL thread id
    th = CreateThread(NULL, 0, awe_thread, NULL, 0, &tid);
}

// ======================================================================================================
// "AWE32 MIDI Synth"
static AwemBank *mbank[3], *mbuiltin;
static uint32_t mbank_ram;               // sample RAM words the banks were loaded for
static AweMidiInfo minfo;
static uint32_t m_seen_rw, m_vm_gone, m_takeovers, m_takebacks;
static DWORD m_last_dos;
static const char* const kSbk[3] = {"SYNTHGM.SBK", "SYNTHGS.SBK", "SYNTHMT.SBK"};
static const char* const kKind[3] = {"GM", "GS", "MT-32"};

void awe_midi_load_cfg(const std::string& ini, AweMidiCfg& c)
{
    char b[16];
    c = AweMidiCfg();
    c.on = GetPrivateProfileIntA("AWE32 MIDI", "Synth", 1, ini.c_str()) != 0;
    GetPrivateProfileStringA("AWE32 MIDI", "Bank", "GM", b, sizeof b, ini.c_str());
    c.bank = !lstrcmpiA(b, "GS") ? 1 : (!lstrcmpiA(b, "MT32") || !lstrcmpiA(b, "MT-32")) ? 2 : 0;
    c.reverb = GetPrivateProfileIntA("AWE32 MIDI", "Reverb", 4, ini.c_str()) & 7;
    c.chorus = GetPrivateProfileIntA("AWE32 MIDI", "Chorus", 2, ini.c_str()) & 7;
    m_ini = ini;
    std::vector<char> sec(32000);   // Windows 98: GetPrivateProfileSection fails above 32767
    DWORD n = GetPrivateProfileSectionA("AWE32 MIDI", sec.data(), (DWORD)sec.size(), ini.c_str());
    for (const char* q = sec.data(); q < sec.data() + n && *q; q += strlen(q) + 1) {
        const char* eq = strchr(q, '=');
        char* e;
        if (!eq) continue;
        if (!_strnicmp(q, "Bank", 4) && q[4] >= '0' && q[4] <= '9') {          // BankN=file (no sscanf: Win98 MSVCRT)
            long a = strtol(q + 4, &e, 10);
            if (e == eq && a >= 1 && a <= 127) c.ubank[a] = eq + 1;
        } else if (!_strnicmp(q, "Preset", 6) && q[6] >= '0' && q[6] <= '9') { // PresetS_P=src_bank,src_prog,file
            long a = strtol(q + 6, &e, 10), b, sb, sp;
            if (*e != '_') continue;
            b = strtol(e + 1, &e, 10);
            if (e != eq) continue;
            sb = strtol(eq + 1, &e, 10);
            if (*e != ',') continue;
            sp = strtol(e + 1, &e, 10);
            if (*e != ',' || a < 0 || a > 128 || b < 0 || b > 127) continue;
            c.presets.push_back({(int)a, (int)b, (int)sb, (int)sp, e + 1});
        }
    }
}
void awe_midi_save_cfg(const std::string& ini, const AweMidiCfg& c)
{
    char b[16];
    WritePrivateProfileStringA("AWE32 MIDI", "Synth", c.on ? "1" : "0", ini.c_str());
    WritePrivateProfileStringA("AWE32 MIDI", "Bank", c.bank == 1 ? "GS" : c.bank == 2 ? "MT32" : "GM", ini.c_str());
    snprintf(b, sizeof b, "%d", c.reverb); WritePrivateProfileStringA("AWE32 MIDI", "Reverb", b, ini.c_str());
    snprintf(b, sizeof b, "%d", c.chorus); WritePrivateProfileStringA("AWE32 MIDI", "Chorus", b, ini.c_str());
}
const AweMidiCfg& awe_midi_cfg() { return mcfg; }
AweMidiInfo awe_midi_info() { awe_lock(); AweMidiInfo r = minfo; awe_unlock(); return r; }

// SoundFont 1 file? (cheap: the first 64 KB must hold RIFF sfbk ... ifil 1.x)
static bool sf1_quick(const std::string& p, DWORD size)
{
    if (size < 512 || size > 64u * 1024 * 1024) return false;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    unsigned char h[4096];
    size_t n = fread(h, 1, sizeof h, f);
    fclose(f);
    if (n < 32 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "sfbk", 4)) return false;
    for (size_t i = 12; i + 10 <= n; i++)
        if (!memcmp(h + i, "ifil", 4)) return h[i + 8] == 1 && h[i + 9] == 0;
    return false;
}
struct Found { std::string path; int kind, rc; std::string name; };
static void scan_dir(const std::string& dir, int depth, std::vector<Found>& out, int& files)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        std::string p = dir + "\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 8) scan_dir(p, depth + 1, out, files); continue; }
        if (++files > 50000) break;
        if (fd.nFileSizeHigh || !sf1_quick(p, fd.nFileSizeLow)) continue;
        int kind = -1; char nm[64];
        int rc = awem_probe(p.c_str(), &kind, nm, sizeof nm);
        if (rc != 1 && rc != 2) continue;
        if (kind < 0) {     // the presets did not tell: the file name, else GM
            std::string u = fd.cFileName;
            for (auto& ch : u) ch = (char)toupper((unsigned char)ch);
            kind = u.find("MT") != std::string::npos ? AWEM_MT32 : u.find("GS") != std::string::npos ? AWEM_GS : AWEM_GM;
        }
        out.push_back({p, kind, rc, nm});
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
static bool same_file(const std::string& a, const std::string& b)
{
    FILE *x = fopen(a.c_str(), "rb"), *y = fopen(b.c_str(), "rb");
    bool same = x && y;
    while (same) {
        char p[4096], q[4096];
        size_t n = fread(p, 1, sizeof p, x), m = fread(q, 1, sizeof q, y);
        if (n != m || memcmp(p, q, n)) same = false;
        if (!n) break;
    }
    if (x) fclose(x);
    if (y) fclose(y);
    return same;
}
std::string awe_midi_scan(const std::string& root, const std::string& romdir)
{
    std::vector<Found> f;
    int files = 0;
    scan_dir(root, 0, f, files);
    std::string r;
    int copied = 0;
    for (int k = 0; k < 3; k++) {
        std::string dst = romdir + "\\" + kSbk[k];
        const Found* best = NULL;
        for (const auto& x : f) if (x.kind == k && (!best || (x.rc == 1 && best->rc != 1))) best = &x;
        if (!best) continue;
        bool have = GetFileAttributesA(dst.c_str()) != INVALID_FILE_ATTRIBUTES;
        if (have && (same_file(best->path, dst) || lstrcmpiA(best->path.c_str(), dst.c_str()) == 0)) { r += std::string(r.empty() ? "" : ", ") + kSbk[k]; continue; }
        if (have) { r += std::string(r.empty() ? "" : ", ") + kSbk[k] + " (kept)"; continue; }
        CreateDirectoryA(romdir.c_str(), NULL);
        if (CopyFileA(best->path.c_str(), dst.c_str(), TRUE)) { copied++; r += std::string(r.empty() ? "" : ", ") + kSbk[k] + " <- " + best->path; }
        else r += std::string(r.empty() ? "" : ", ") + "could not copy " + best->path;
    }
    char b[96];
    snprintf(b, sizeof b, "%d files searched, %d SoundFont 1 bank(s) found, %d copied to ROMS", files, (int)f.size(), copied);
    return std::string(b) + (r.empty() ? "" : ": " + r);
}

static void apply_banks()            // locked; engine up
{
    AwemBank* list[4];
    int n = 0, mode = m_dos_mode >= 0 ? m_dos_mode : mcfg.bank;
    if (mbank[mode]) list[n++] = mbank[mode];
    if (mode != 0 && mbank[0]) list[n++] = mbank[0];
    minfo.builtin = n == 0;
    if (!n) { if (!mbuiltin) mbuiltin = awem_bank_builtin(); if (mbuiltin) list[n++] = mbuiltin; }
    minfo.using_.clear();
    for (int i = 0; i < n; i++) {
        if (i) minfo.using_ += " + ";
        minfo.using_ += list[i] == mbuiltin ? std::string("NUKED's own GM map") : std::string(awem_bank_name(list[i]));
        for (int k = 0; k < 3; k++) if (list[i] == mbank[k]) minfo.using_ += std::string(" (") + kSbk[k] + ")";
    }
    ram_sync();                       // the user banks' room (layout)
    awem_set_banks(list, n, mode);
    if (!m_started) { awem_start(); awem_reset(); m_started = true; }
    awem_set_fx(mcfg.reverb, mcfg.chorus);
}
static void load_banks(const std::string& romdir, uint32_t ram_words, AwemBank* nb[3], std::string& note)
{
    for (int k = 0; k < 3; k++) {
        nb[k] = NULL;
        std::string p = romdir + "\\" + kSbk[k];
        if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        char err[128] = "";
        nb[k] = awem_bank_load(p.c_str(), ram_words, err, sizeof err);
        if (!nb[k]) note += std::string(kSbk[k]) + ": " + err + ". ";
        else if (awem_bank_skipped(nb[k])) {
            char b[160];
            snprintf(b, sizeof b, "%s: %d sounds left out (sample RAM too small: AWE32 page, Sample RAM). ", kSbk[k], awem_bank_skipped(nb[k]));
            note += b;
        }
    }
}
static std::string m_romdir;
static void sync_users(const AweMidiCfg& c);
static void ram_sync()
{ static uint32_t cur = 0xFFFFFFFFu; if (cur != mbank_ram) { cur = mbank_ram; awem_ram_set(cur); } }   // locked
void awe_midi_config(const AweMidiCfg& c, const std::string& romdir, int ram_kb)
{
    uint32_t ram = (uint32_t)(info.on ? info.ram_kb : ram_kb) * 512u;
    AwemBank* nb[3];
    std::string note;
    load_banks(romdir, ram, nb, note);
    awe_lock();
    for (int k = 0; k < 3; k++) { awem_bank_free(mbank[k]); mbank[k] = nb[k]; minfo.file[k] = nb[k] ? romdir + "\\" + kSbk[k] : ""; }
    mbank_ram = ram;
    m_romdir = romdir;
    mcfg = c;
    minfo.note = note;
    if (info.on && want_engine()) apply_banks();
    else if (m_started && !want_engine()) { awem_release(); m_started = false; }
    awe_unlock();
    sync_users(c);
}
static void midi_engine_up()         // locked: a new engine (emu8k_init_mem done, ROM loaded)
{
    m_started = false;
    m_seen_rw = 0; m_vm_gone = 0;
    if (!want_engine()) return;
    uint32_t ram = (uint32_t)info.ram_kb * 512u;
    if (ram != mbank_ram && !m_romdir.empty()) {   // reload for the new sample RAM size
        AwemBank* nb[3];
        std::string note;
        load_banks(m_romdir, ram, nb, note);
        for (int k = 0; k < 3; k++) { awem_bank_free(mbank[k]); mbank[k] = nb[k]; }
        mbank_ram = ram;
        minfo.note = note;
    }
    apply_banks();
}
static void midi_engine_down() { if (m_started) awem_release(); m_started = false; }
bool awe_midi_ready() { return m_started && info.on; }
void awe_midi_byte(uint8_t b) { if (m_started) awem_byte(b); }
bool awe_midi_windows() { return m_started && info.on && !awem_released(); }
void awe_midi_counts(uint32_t& takeovers, uint32_t& takebacks) { takeovers = m_takeovers; takebacks = m_takebacks; }
void awe_midi_poll(bool dos_wrote)
{
    if (!m_started || !B || !info.port) return;
    uint32_t rw = B->reads + B->writes;
    DWORD now = GetTickCount();
    if (dos_wrote || rw != m_seen_rw) {
        m_seen_rw = rw;
        m_last_dos = now;
        if (!awem_released()) { awem_release(); m_vm_gone = B->vm_gone; m_takeovers++; }
        return;
    }
    if (awem_released() && ((B->vm == 0 && B->vm_gone != m_vm_gone) || now - m_last_dos > 10000)) {
        awem_reclaim();
        m_takebacks++;
    }
    if (m_dos_mode >= 0 && now - m_dos_last > 10000) { int was = m_dos_mode; m_dos_mode = -1; if (was != mcfg.bank) apply_banks(); awem_reset(); }   // back to Windows' MIDI: clean
}
void awe_midi_dos_byte(uint8_t b, int bank)
{
    if (!m_started) return;
    m_dos_last = GetTickCount();
    if (bank != m_dos_mode) {      // a DOS box starts playing: its own MIDI state (GM reset), its bank
        int was = m_dos_mode < 0 ? mcfg.bank : m_dos_mode;
        bool fresh = m_dos_mode < 0;
        m_dos_mode = bank;
        if (bank != was) apply_banks();
        if (fresh) awem_reset();
    }
    awem_byte(b);
}
std::string awe_midi_stats()
{
    AwemStats s;
    char b[480];
    awe_lock();
    if (!m_started) { awe_unlock(); return ""; }
    awem_stats(&s);
    snprintf(b, sizeof b, "AWE32 MIDI Synth: %u bytes, %u notes (%u from user banks), %u voices at most, %u stolen, %u without a sound, %u while a DOS box had "
             "the EMU8000 (DOS took it %u times, back %u)%s; sample RAM %u of %u KB used%s", s.bytes, s.notes, s.user_notes, s.voices_max, s.stolen, s.missing, s.released_notes,
             m_takeovers, m_takebacks, awem_released() ? ", DOS box has it now" : "", (awem_ram_used() + 511) / 512, awem_ram_total() / 512,
             m_dos_mode >= 0 ? (m_dos_mode == 1 ? ", DOS MPU-401: GS" : m_dos_mode == 2 ? ", DOS MPU-401: MT-32" : ", DOS MPU-401: GM") : "");
    awe_unlock();
    return b;
}

// ======================================================================================================
// bank manager: user banks 1-127 and single presets (awemidi.c keeps them and their sample RAM); files are
// read outside the lock, the RAM layout + upload happen locked. SC55W.INI [AWE32 MIDI] BankN= / PresetS_P=.
static void users_info()             // locked
{
    for (int i = 1; i < 128; i++) {
        const AwemBank* b = awem_ubank(i);
        if (b) { minfo.uname[i] = awem_bank_name(b); minfo.ukb[i] = (awem_bank_ram_used(b) + 511) / 512;
                 if (!awem_bank_loaded(b)) minfo.uerr[i] = "left out: the sample RAM is too small now"; }
        else { minfo.uname[i].clear(); minfo.ukb[i] = 0; }
    }
    minfo.prow.clear();
    for (int i = 0; i < awem_preset_count(); i++) {
        int sl, pg;
        const AwemBank* b = awem_preset_get(i, &sl, &pg);
        std::string file;
        for (const auto& x : m_ppath) if (x.slot == sl && x.prog == pg) { size_t k = x.path.find_last_of("\\/"); file = k == std::string::npos ? x.path : x.path.substr(k + 1); }
        char t[200];
        if (sl == 128) snprintf(t, sizeof t, "Drum kit %d: %s <- %s (%u KB)%s", pg, awem_bank_name(b), file.c_str(), (awem_bank_ram_used(b) + 511) / 512, awem_bank_loaded(b) ? "" : " - left out, RAM full");
        else snprintf(t, sizeof t, "Bank %d program %d: %s <- %s (%u KB)%s", sl, pg, awem_bank_name(b), file.c_str(), (awem_bank_ram_used(b) + 511) / 512, awem_bank_loaded(b) ? "" : " - left out, RAM full");
        minfo.prow.push_back(t);
    }
    minfo.ram_kb = awem_ram_total() / 512;
    minfo.used_kb = (awem_ram_used() + 511) / 512;
}
static std::string put_bank(int slot, const std::string& path)    // not locked
{
    char err[200] = "";
    AwemBank* b = awem_bank_load(path.c_str(), 0xFFFFFFFFu, err, sizeof err);
    if (b && !awem_bank_presets(b)) { awem_bank_free(b); b = NULL; snprintf(err, sizeof err, "no presets in the file"); }
    awe_lock();
    ram_sync();
    int rc = b ? awem_ubank_put(slot, b, err, sizeof err) : -1;
    if (rc && b) awem_bank_free(b);
    minfo.uerr[slot] = rc ? err : "";
    if (!rc) m_upath[slot] = path;
    users_info();
    awe_unlock();
    return rc ? std::string(err) : std::string();
}
static std::string put_preset(const AwePresetCfg& p)                // not locked
{
    char err[200] = "";
    AwemBank* src = awem_bank_load(p.path.c_str(), 0xFFFFFFFFu, err, sizeof err);
    awe_lock();
    ram_sync();
    int rc = src ? awem_preset_put(p.slot, p.prog, src, p.src_bank, p.src_prog, err, sizeof err) : -1;
    if (!rc) {
        bool f = false;
        for (auto& x : m_ppath) if (x.slot == p.slot && x.prog == p.prog) { x = p; f = true; }
        if (!f) m_ppath.push_back(p);
    }
    users_info();
    awe_unlock();
    awem_bank_free(src);
    return rc ? std::string(err) : std::string();
}
static void sync_users(const AweMidiCfg& c)    // not locked: load what changed (start: all of them)
{
    std::string log;
    for (int i = 1; i < 128; i++) {
        if (c.ubank[i] == m_upath[i] && (c.ubank[i].empty() || minfo.uerr[i].empty())) continue;
        if (c.ubank[i].empty()) { awe_lock(); awem_ubank_remove(i); m_upath[i].clear(); minfo.uerr[i].clear(); users_info(); awe_unlock(); continue; }
        std::string e = put_bank(i, c.ubank[i]);
        char t[64]; snprintf(t, sizeof t, "bank %d: ", i);
        log += t + (e.empty() ? minfo.uname[i] : e) + ". ";
    }
    for (const auto& p : c.presets) {
        bool have = false;
        for (const auto& x : m_ppath) have |= x.slot == p.slot && x.prog == p.prog && x.path == p.path && x.src_bank == p.src_bank && x.src_prog == p.src_prog;
        if (have) continue;
        std::string e = put_preset(p);
        if (!e.empty()) { char t[64]; snprintf(t, sizeof t, "preset %d/%d: ", p.slot, p.prog); log += t + e + ". "; }
    }
    awe_lock(); users_info(); awe_unlock();
    if (!log.empty()) {
        char t[96]; snprintf(t, sizeof t, " Sample RAM %u of %u KB used.", minfo.used_kb, minfo.ram_kb);
        awe_lock(); minfo.note += "User banks: " + log + t; awe_unlock();
    }
}
static void ini_put(const char* key, const std::string& v) { if (!m_ini.empty()) WritePrivateProfileStringA("AWE32 MIDI", key, v.empty() ? NULL : v.c_str(), m_ini.c_str()); }
std::string awe_bank_set(int slot, const std::string& path)
{
    if (slot < 1 || slot > 127) return "user banks are 1-127 (bank 0 = the GM/GS/MT-32 bank)";
    std::string e = put_bank(slot, path);
    if (e.empty()) { mcfg.ubank[slot] = path; char k[16]; snprintf(k, sizeof k, "Bank%d", slot); ini_put(k, path); }
    return e;
}
void awe_bank_remove(int slot)
{
    if (slot < 1 || slot > 127) return;
    awe_lock(); awem_ubank_remove(slot); m_upath[slot].clear(); minfo.uerr[slot].clear(); users_info(); awe_unlock();
    mcfg.ubank[slot].clear();
    char k[16]; snprintf(k, sizeof k, "Bank%d", slot); ini_put(k, "");
}
std::string awe_preset_set(int slot, int prog, const std::string& path, int src_bank, int src_prog)
{
    AwePresetCfg p{slot, prog, src_bank, src_prog, path};
    std::string e = put_preset(p);
    if (!e.empty()) return e;
    bool f = false;
    for (auto& x : mcfg.presets) if (x.slot == slot && x.prog == prog) { x = p; f = true; }
    if (!f) mcfg.presets.push_back(p);
    char k[32], v[300];
    snprintf(k, sizeof k, "Preset%d_%d", slot, prog); snprintf(v, sizeof v, "%d,%d,%s", src_bank, src_prog, path.c_str());
    ini_put(k, v);
    return "";
}
void awe_preset_remove(int slot, int prog)
{
    awe_lock();
    awem_preset_remove(slot, prog);
    for (size_t i = 0; i < m_ppath.size(); i++) if (m_ppath[i].slot == slot && m_ppath[i].prog == prog) { m_ppath.erase(m_ppath.begin() + i); break; }
    users_info();
    awe_unlock();
    for (size_t i = 0; i < mcfg.presets.size(); i++) if (mcfg.presets[i].slot == slot && mcfg.presets[i].prog == prog) { mcfg.presets.erase(mcfg.presets.begin() + i); break; }
    char k[32]; snprintf(k, sizeof k, "Preset%d_%d", slot, prog); ini_put(k, "");
}
std::string awe_file_presets(const std::string& path, std::vector<std::string>& names, std::vector<int>& bank, std::vector<int>& prog)
{
    char err[200] = "";
    names.clear(); bank.clear(); prog.clear();
    AwemBank* b = awem_bank_load(path.c_str(), 0, err, sizeof err);     // no RAM: names only
    if (!b) return err;
    int bk, pg;
    const char* nm;
    for (int i = 0; (nm = awem_bank_preset(b, i, &bk, &pg)) != NULL; i++) {
        char t[64];
        snprintf(t, sizeof t, "%s %d:%d %s", bk == 128 ? "drums" : "bank", bk, pg, nm);
        names.push_back(t); bank.push_back(bk); prog.push_back(pg);
    }
    awem_bank_free(b);
    return names.empty() ? "no presets in the file" : "";
}
