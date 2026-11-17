/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sc55sf)
 * oracle: runs our Nuked-SC55 fork and traces what the firmware does while it plays, to decode the control ROM.
 * Usage: oracle ROMDIR SCRIPT [romset] > trace.txt
 * poke <hexaddr> <hexval...>  override control-ROM bytes | log 3 = only S lines: state of keyed voices every 64 frames
 *   S t slot pitch(r2[0]) pan(r2[1]) send(r2[2]) ctl(r2[6]) tva(r2[9]) amp2(r2[10]) tvf(r2[11]) addr(r1[4]) r2[7]
 * Script lines:  m <hex bytes...>   send MIDI     |  w <ms>   render ms     |  log 0|1|2   (1 = PCM writes, 2 = + ROM reads)
 *                wav <file>   start writing the output (native rate, stereo s16)   |  # comment
 * Trace lines (time = output frames at the native rate):  P t kind slot ix val   R t addr(first) n(consecutive) */
#include "sc55synth.h"
#include "pcm.h"
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
static long g_frames; static int g_log;
static uint32_t r_first = 0xffffffff, r_last, r_n;
static void rflush(){ if(r_n) printf("R %ld %05x %u\n", g_frames, r_first, r_n); r_n = 0; r_first = 0xffffffff; }
void (*sc55sf_pcmw)(int, int, int, uint32_t); void* sc55sf_pcm; static std::map<uint32_t,uint8_t> g_poke;
uint8_t sc55sf_r2(const uint8_t* rom, uint32_t a){
  if (g_log >= 2 && a >= 0x10000) { if (r_n && a == r_last + 1) { r_last = a; r_n++; } else { rflush(); r_first = r_last = a; r_n = 1; } }
  if (!g_poke.empty()) { auto it = g_poke.find(a); if (it != g_poke.end()) return it->second; }
  return rom[a]; }
static void pw(int k, int s, int ix, uint32_t v){ if(g_log == 3 && k == 1 && ix == 4) printf("N %ld %d %x\n", g_frames, s, v); if(g_log == 1 || g_log == 2){ rflush(); printf("P %ld %d %d %d %x\n", g_frames, k, s, ix, v);} }
int main(int argc, char** argv){
  if(argc < 3){ fprintf(stderr, "oracle ROMDIR SCRIPT [romset]\n"); return 1; }
  SC55_Config cfg = {argv[1], argc > 3 ? argv[3] : "auto", 100, 0, 0};
  if(!SC55_Init(&cfg)){ fprintf(stderr, "init: %s\n", SC55_LastError()); return 1; }
  sc55sf_pcmw = pw; uint32_t rate = SC55_NativeRate(); fprintf(stderr, "%s rate %u\n", SC55_ModelName(), rate);
  FILE* sc = fopen(argv[2], "r"); FILE* wav = 0; char line[4096]; int16_t buf[128]; long wframes = 0;
  auto render = [&](double ms){ long n = (long)(ms * rate / 1000.0); while(n > 0){ int b = n > 64 ? 64 : (int)n; SC55_Render(buf, b, rate, 0);
      if(wav){ fwrite(buf, 4, b, wav); wframes += b; } g_frames += b; n -= b;
      if(g_log == 3 && sc55sf_pcm){ pcm_t& P = *(pcm_t*)sc55sf_pcm; for(int s = 0; s < 32; s++) if(P.ram2[s][7] & 0x20)
        printf("S %ld %d %x %x %x %x %x %x %x %x %x\n", g_frames, s, P.ram2[s][0], P.ram2[s][1], P.ram2[s][2], P.ram2[s][6], P.ram2[s][9], P.ram2[s][10], P.ram2[s][11], P.ram1[s][4], P.ram2[s][7]); } } };
  while(fgets(line, sizeof line, sc)){
    if(line[0] == 'm'){ char* p = line + 1; long v; char* e; while((v = strtol(p, &e, 16)), e != p){ while(!SC55_PostByte((uint8_t)v)) render(1); p = e; } }
    else if(line[0] == 'w' && line[1] == ' ') render(atof(line + 2));
    else if(!strncmp(line, "poke", 4)){ char* p = line + 4; char* e; uint32_t a = strtoul(p, &e, 16); p = e; long v; while((v = strtol(p, &e, 16)), e != p){ g_poke[a++] = (uint8_t)v; p = e; } }
    else if(!strncmp(line, "unpoke", 6)) g_poke.clear();
    else if(!strncmp(line, "log", 3)){ rflush(); g_log = atoi(line + 3); }
    else if(!strncmp(line, "wav", 3)){ char fn[1000]; sscanf(line + 3, "%999s", fn); wav = fopen(fn, "wb"); static uint8_t h[44] = {0}; fwrite(h, 1, 44, wav); }
  }
  rflush();
  if(wav){ uint32_t d = wframes * 4; uint8_t h[44]; memcpy(h, "RIFF", 4); uint32_t x = d + 36; memcpy(h + 4, &x, 4); memcpy(h + 8, "WAVEfmt ", 8);
    x = 16; memcpy(h + 16, &x, 4); uint16_t s = 1; memcpy(h + 20, &s, 2); s = 2; memcpy(h + 22, &s, 2); memcpy(h + 24, &rate, 4); x = rate * 4; memcpy(h + 28, &x, 4);
    s = 4; memcpy(h + 32, &s, 2); s = 16; memcpy(h + 34, &s, 2); memcpy(h + 36, "data", 4); memcpy(h + 40, &d, 4); fseek(wav, 0, SEEK_SET); fwrite(h, 1, 44, wav); fclose(wav); }
  return 0; }
