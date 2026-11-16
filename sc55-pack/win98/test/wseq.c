/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-AWE */
/* wseq.c - AWE line test: a small MIDI sequencer for Windows 98. Plays a Standard MIDI File through the Windows
 * MIDI stream API (midiStreamOut: what sequencers use; SysEx as long messages) on the device whose name contains
 * DEVICE ("MAPPER" = the MIDI Mapper), at VOL percent (midiOutSetVolume), and logs to C:\WSEQ.LOG what it sent:
 *   WSEQ DEVICE FILE.MID [VOL] [SECONDS]      e.g. WSEQ AWE32 C:\T\SONG.MID 25 60   (SECONDS: stop after that) */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { DWORD tick; DWORD ev; int n; const BYTE* data; } Ev;
static Ev* ev; static int nev, cev;
static void add(DWORD tick, DWORD e, const BYTE* data, int n)
{
    if (nev == cev) { cev = cev ? cev * 2 : 4096; ev = (Ev*)realloc(ev, cev * sizeof(Ev)); }
    ev[nev].tick = tick; ev[nev].ev = e; ev[nev].data = data; ev[nev].n = n; nev++;
}
static DWORD vlq(const BYTE** p, const BYTE* e) { DWORD v = 0; while (*p < e) { BYTE b = *(*p)++; v = v << 7 | (b & 127); if (!(b & 128)) break; } return v; }
static int cmpev(const void* a, const void* b)
{
    const Ev *x = (const Ev*)a, *y = (const Ev*)b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return x < y ? -1 : 1;
}
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    char dev[64] = "", file[MAX_PATH] = "";
    int vol = 100, secs = 0, i, ntr, div, notes = 0, bytes = 0, sysex = 0;
    FILE *lg = fopen("C:\\WSEQ.LOG", "a"), *f;
    BYTE *d; long sz; const BYTE *p, *e;
    UINT id = MIDI_MAPPER, nd = midiOutGetNumDevs();
    HMIDISTRM hs; MIDIPROPTIMEDIV td; MMRESULT r;
    DWORD *buf; int nbuf = 0, cap; MIDIHDR* hdr; int nhdr = 0; DWORD t0;
    (void)hi; (void)hp; (void)show;
    {   /* no sscanf: MinGW's pulls _strtoui64, which Windows 98's MSVCRT lacks */
        char* t = strtok(cmd, " "); if (t) lstrcpynA(dev, t, sizeof dev);
        t = strtok(NULL, " "); if (t) lstrcpynA(file, t, sizeof file);
        t = strtok(NULL, " "); if (t) vol = atoi(t);
        t = strtok(NULL, " "); if (t) secs = atoi(t);
    }
    if (!lg) return 1;
    if (strcmp(dev, "MAPPER")) {
        for (i = 0; i < (int)nd; i++) {
            MIDIOUTCAPSA c;
            if (midiOutGetDevCapsA(i, &c, sizeof c) == 0 && strstr(c.szPname, dev)) { id = i; break; }
        }
        if (id == MIDI_MAPPER) { fprintf(lg, "WSEQ: no MIDI out device with '%s' (%u devices)\n", dev, nd); fclose(lg); return 2; }
    }
    f = fopen(file, "rb");
    if (!f) { fprintf(lg, "WSEQ: cannot open %s\n", file); fclose(lg); return 3; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    d = (BYTE*)malloc(sz); fread(d, 1, sz, f); fclose(f);
    if (memcmp(d, "MThd", 4)) { fprintf(lg, "WSEQ: %s is not a MIDI file\n", file); fclose(lg); return 3; }
    ntr = d[10] << 8 | d[11]; div = d[12] << 8 | d[13];
    p = d + 8 + (d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7]);
    for (i = 0; i < ntr && p + 8 <= d + sz; i++) {
        DWORD len = (DWORD)(p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7]), t = 0; BYTE rs = 0;
        const BYTE* q = p + 8;
        e = q + len; if (e > d + sz) e = d + sz;
        while (q < e) {
            BYTE b;
            t += vlq(&q, e); if (q >= e) break;
            b = *q;
            if (b == 0xFF) { BYTE ty = q[1]; DWORD l; q += 2; l = vlq(&q, e);
                if (ty == 0x51 && l == 3) add(t, ((DWORD)MEVT_TEMPO << 24) | (q[0] << 16 | q[1] << 8 | q[2]), NULL, 0);
                q += l; continue; }
            if (b == 0xF0 || b == 0xF7) { DWORD l; const BYTE* s0 = q; q++; l = vlq(&q, e);
                if (b == 0xF0) { add(t, 0xFFFFFFFF, q, (int)l); sysex++; }   /* F0 + l data bytes (F7 included) */
                (void)s0; q += l; rs = 0; continue; }
            if (b & 0x80) { rs = b; q++; }
            if (!rs) { q++; continue; }
            {
                DWORD m = rs | (q[0] << 8);
                q++;
                if ((rs & 0xE0) != 0xC0) { m |= q[0] << 16; q++; }
                if ((rs & 0xF0) == 0x90 && (m >> 16) & 0x7F) notes++;
                bytes += (rs & 0xE0) == 0xC0 ? 2 : 3;
                add(t, ((DWORD)MEVT_SHORTMSG << 24) | m, NULL, 0);
            }
        }
        p = e;
    }
    qsort(ev, nev, sizeof(Ev), cmpev);
    r = midiStreamOpen(&hs, &id, 1, 0, 0, CALLBACK_NULL);
    if (r) { fprintf(lg, "WSEQ: midiStreamOpen device %u failed (%u)\n", id, r); fclose(lg); return 4; }
    td.cbStruct = sizeof td; td.dwTimeDiv = div;
    midiStreamProperty(hs, (LPBYTE)&td, MIDIPROP_SET | MIDIPROP_TIMEDIV);
    {
        DWORD v = (DWORD)(vol * 655) & 0xFFFF, got = 0;
        v |= v << 16;
        r = midiOutSetVolume((HMIDIOUT)hs, v);
        midiOutGetVolume((HMIDIOUT)hs, &got);
        fprintf(lg, "WSEQ: device %u, %s, %d events, %d note-ons, %d bytes + %d SysEx, volume %d%% (set %u, reads %08lX)\n",
                id, file, nev, notes, bytes, sysex, vol, r, got);
    }
    /* the whole song in headers of <= 60 KB: delta, stream id, event (+ SysEx data padded to 4 bytes) */
    cap = 60000 / 4;
    hdr = (MIDIHDR*)calloc(nev / 1000 + sysex + 8, sizeof(MIDIHDR));
    buf = (DWORD*)malloc(cap * 4);
    {
        DWORD last = 0;
        for (i = 0; i <= nev; i++) {
            int need = i < nev ? 3 + (ev[i].ev == 0xFFFFFFFF ? (ev[i].n + 1 + 3) / 4 : 0) : 0;
            if (i == nev || nbuf + need > cap) {
                MIDIHDR* h = &hdr[nhdr++];
                h->lpData = (LPSTR)buf; h->dwBufferLength = h->dwBytesRecorded = nbuf * 4;
                midiOutPrepareHeader((HMIDIOUT)hs, h, sizeof *h);
                midiStreamOut(hs, h, sizeof *h);
                if (i == nev) break;
                buf = (DWORD*)malloc(cap * 4); nbuf = 0;
            }
            buf[nbuf++] = ev[i].tick - last; last = ev[i].tick;
            buf[nbuf++] = 0;
            if (ev[i].ev == 0xFFFFFFFF) {
                BYTE* s = (BYTE*)&buf[nbuf + 1];
                buf[nbuf++] = ((DWORD)MEVT_LONGMSG << 24) | (ev[i].n + 1);
                s = (BYTE*)&buf[nbuf]; s[0] = 0xF0; memcpy(s + 1, ev[i].data, ev[i].n);
                nbuf += (ev[i].n + 1 + 3) / 4;
            } else buf[nbuf++] = ev[i].ev;
        }
    }
    t0 = GetTickCount();
    midiStreamRestart(hs);
    while (!(hdr[nhdr - 1].dwFlags & MHDR_DONE)) {
        Sleep(50);
        if (secs && GetTickCount() - t0 > (DWORD)secs * 1000) break;
    }
    midiOutReset((HMIDIOUT)hs);
    for (i = 0; i < nhdr; i++) midiOutUnprepareHeader((HMIDIOUT)hs, &hdr[i], sizeof hdr[i]);
    midiStreamClose(hs);
    fprintf(lg, "WSEQ: played %.1f s%s\n", (GetTickCount() - t0) / 1000.0, secs && GetTickCount() - t0 > (DWORD)secs * 1000 ? " (stopped)" : "");
    fclose(lg);
    return 0;
}
