/* UNPAK.EXE - unpacks the NUKED SB-EMU floppy set (one xz stream per file, CRC32, files may span disks).
   usage: UNPAK A:\DISK1.PAK destdir    asks for each next disk itself; on a damaged disk it asks for
                                        that disk again and redoes the file.  /T instead of destdir = test only.
   exit: 0 ok, 2 stopped (Esc), 3 cannot write (disk full?), 1 usage
   Uses XZ Embedded (0BSD). Part of NUKED SB-EMU, GPL-2.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "xz.h"
#ifdef __DJGPP__
#include <conio.h>
#define WAITKEY() getch()
#else
#define WAITKEY() (getenv("UNPAK_NOWAIT") ? 0 : getchar())
#endif
#define INB 16384
#define OUTB 32768
static unsigned char inb[INB], outb[OUTB];
static char pat[300];              /* "A:\DISK%u.PAK" */
static FILE *vf; static unsigned vdisk, vndisks, vset, vstart, vlen, vpos;   /* current volume */
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | (unsigned) p[2] << 16 | (unsigned) p[3] << 24; }
static void stop(void) { puts("\nStopped. Run INSTALL again to start over."); exit(2); }
static void waitkey(void) { int c = WAITKEY(); if (c == 27 || c == 3) stop(); }
/* open volume n so that stream position spos can be read; prompts until the right disk is in */
static void openvol(unsigned n, unsigned spos, const char *why)
{
    char path[300]; unsigned char h[20];
    if (vf) { fclose(vf); vf = NULL; }
    for (;;) {
        FILE *f; snprintf(path, sizeof path, pat, n);
        if (why) { printf("\n %s\n Insert disk %u in drive %c: and press any key  (Esc = stop)\n", why, n, pat[0]); waitkey(); }
        why = "";
        if (!(f = fopen(path, "rb"))) { why = "Disk not found or not readable."; continue; }
        if (fread(h, 1, 20, f) != 20 || memcmp(h, "NSBVOL2", 8)) { fclose(f); why = "That is not a disk of this set."; continue; }
        if (h[8] != n || (vset && rd32(h + 12) != vset)) { fclose(f); why = "Wrong disk."; continue; }
        vdisk = n; vndisks = h[9]; vset = rd32(h + 12); vstart = rd32(h + 16);
        fseek(f, 0, SEEK_END); vlen = ftell(f) - 20;
        if (spos < vstart || spos > vstart + vlen) { fclose(f); why = "Wrong disk."; continue; }
        fseek(f, 20 + spos - vstart, SEEK_SET); vpos = spos; vf = f; return;
    }
}
/* read k bytes at the current stream position; 0 = read error on this disk */
static int rd(unsigned char *b, unsigned k)
{
    while (k) {
        unsigned left = vstart + vlen - vpos, t = k < left ? k : left;
        if (!left) { openvol(vdisk + 1, vpos, ""); continue; }
        if (fread(b, 1, t, vf) != t) return 0;
        b += t; k -= t; vpos += t;
    }
    return 1;
}
static int mkdirs(char *path)
{
    char *p;
    for (p = path + 1; *p; p++)
        if (*p == '\\' || *p == '/') {
            char c = *p; struct stat st; *p = 0;
            if (mkdir(path, 0777) && stat(path, &st)) { *p = c; return -1; }
            *p = c;
        }
    return 0;
}
int main(int argc, char **argv)
{
    unsigned char h[8]; unsigned n, i; unsigned long total = 0; int test; const char *dst; char *d;
    if (argc != 3) { puts("usage: UNPAK A:\\DISK1.PAK destdir   (or /T instead of destdir: test the disks)"); return 1; }
    test = !strcmp(argv[2], "/T") || !strcmp(argv[2], "/t"); dst = argv[2];
    snprintf(pat, sizeof pat - 4, "%s", argv[1]);
    d = strrchr(pat, '1'); if (!d) { puts("name must be ...DISK1.PAK"); return 1; }
    memmove(d + 2, d + 1, strlen(d + 1) + 1); d[0] = '%'; d[1] = 'u';
    xz_crc32_init();
    openvol(1, 0, NULL);
    if (!rd(h, 8) || memcmp(h, "NSBPAK2", 7)) { puts("Disk 1 unreadable or not disk 1 of this set."); return 2; }
    if (!rd(h, 2)) { puts("Disk 1 read error."); return 2; }
    n = h[0] | h[1] << 8;
    printf("%u files on %u disks%s\n", n, vndisks, test ? " (test only, nothing is written)" : "");
    for (i = 0; i < n; i++) {
        unsigned char e[10]; char name[200], path[400]; unsigned nl, size, xl, got, sdisk, spos;
        struct xz_dec *s; struct xz_buf b; enum xz_ret r; FILE *o;
    again:
        *name = 0; sdisk = vdisk; spos = vpos; got = 0; o = NULL; r = XZ_OK;
        if (!rd(e, 2) || (nl = e[0] | e[1] << 8) >= sizeof name || !rd((unsigned char *) name, nl) || !rd(e, 8)) goto bad;
        name[nl] = 0; size = rd32(e); xl = rd32(e + 4);
        snprintf(path, sizeof path, "%s\\%s", dst, name);
        printf("  %-46s %8u\r", name, size); fflush(stdout);
        if (!test && (mkdirs(path) || !(o = fopen(path, "wb")))) { printf("\nCannot create %s\n", path); return 3; }
        if (!(s = xz_dec_init(XZ_DYNALLOC, 1u << 26))) { puts("\nOut of memory"); return 3; }
        b.in = inb; b.in_pos = b.in_size = 0; b.out = outb; b.out_pos = 0; b.out_size = OUTB;
        while (r == XZ_OK) {
            if (b.in_pos == b.in_size && xl) {
                unsigned k = xl < INB ? xl : INB;
                if (!rd(inb, k)) { r = XZ_DATA_ERROR; break; }
                b.in_size = k; b.in_pos = 0; xl -= k;
            }
            r = xz_dec_run(s, &b);
            if (b.out_pos == OUTB || r == XZ_STREAM_END) {
                if (o && fwrite(outb, 1, b.out_pos, o) != b.out_pos) { printf("\nWrite error: %s (disk full?)\n", path); return 3; }
                got += b.out_pos; b.out_pos = 0;
            }
            if (r == XZ_OK && b.in_pos == b.in_size && !xl) r = XZ_DATA_ERROR;
        }
        xz_dec_end(s);
        if (o && fclose(o)) { printf("\nWrite error: %s (disk full?)\n", path); return 3; }
        if (r != XZ_STREAM_END || got != size || xl || b.in_pos != b.in_size) {
        bad: ;
            {   char why[300]; unsigned bd = vdisk;
                if (sdisk == bd) snprintf(why, sizeof why, "Disk %u is damaged%s%s. Write disk %u again.", bd, *name ? " in " : "", name, bd);
                else snprintf(why, sizeof why, "Data error in %s (disks %u-%u): write those disks again.", name, sdisk, bd);
                if (o) remove(path);
                openvol(sdisk, spos, why); *name = 0; goto again; }
        }
        total += size; printf("  %-46s %8u ok\n", name, size);
    }
    if (vf) fclose(vf);
    printf("All %u disks ok, %lu bytes.\n", vndisks, total);
    return 0;
}
