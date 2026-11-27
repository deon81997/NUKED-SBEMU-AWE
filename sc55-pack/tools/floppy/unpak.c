/* UNPAK.EXE - unpacks one DISKn.PAK of the NUKED SB-EMU floppy set (xz streams, CRC32 checked).
   usage: UNPAK disk.pak destdir      unpack (creates folders)
          UNPAK /T disk.pak           test the disk only (read + decode everything, write nothing)
   exit: 0 ok, 10 ok and last disk, 1 usage, 2 bad/unreadable disk, 3 cannot write (disk full?)
   Uses XZ Embedded (0BSD). Part of NUKED SB-EMU, GPL-2.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include "xz.h"
#define INB 16384
#define OUTB 32768
static unsigned char inb[INB], outb[OUTB];
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | (unsigned) p[2] << 16 | (unsigned) p[3] << 24; }
static int mkdirs(char *path)          /* create every folder of path (not the last part) */
{
    char *p;
    for (p = path + 1; *p; p++)
        if (*p == '\\' || *p == '/') {
            char c = *p; *p = 0;
            if (mkdir(path, 0777) && errno != EEXIST) { struct stat st; if (stat(path, &st)) { *p = c; return -1; } }
            *p = c;
        }
    return 0;
}
int main(int argc, char **argv)
{
    int test = 0, ai = 1; FILE *f; unsigned char h[12]; unsigned n, i, total = 0; const char *dst = "";
    if (argc > 1 && (!strcmp(argv[1], "/T") || !strcmp(argv[1], "/t"))) { test = 1; ai = 2; }
    if (argc - ai != (test ? 1 : 2)) { puts("usage: UNPAK disk.pak destdir   or   UNPAK /T disk.pak"); return 1; }
    if (!test) dst = argv[ai + 1];
    xz_crc32_init();
    if (!(f = fopen(argv[ai], "rb")) || fread(h, 1, 12, f) != 12 || memcmp(h, "NSBPAK1", 8)) {
        printf("Cannot read %s (wrong disk or bad floppy)\n", argv[ai]); return 2; }
    n = h[10] | h[11] << 8;
    printf("Disk %u of %u, %u files%s\n", h[8], h[9], n, test ? " (test only)" : "");
    for (i = 0; i < n; i++) {
        unsigned char e[10]; char name[200], path[400]; unsigned nl, size, xl, got = 0; FILE *o = NULL;
        struct xz_dec *s; struct xz_buf b; enum xz_ret r = XZ_OK;
        if (fread(e, 1, 2, f) != 2 || (nl = e[0] | e[1] << 8) >= sizeof name || fread(name, 1, nl, f) != nl || fread(e, 1, 8, f) != 8) {
            puts("\nRead error: bad floppy?"); return 2; }
        name[nl] = 0; size = rd32(e); xl = rd32(e + 4);
        snprintf(path, sizeof path, "%s%s%s", dst, *dst ? "\\" : "", name);
        printf("  %-46s %8u\r", name, size); fflush(stdout);
        if (!test) {
            if (mkdirs(path) || !(o = fopen(path, "wb"))) { printf("\nCannot create %s\n", path); return 3; }
        }
        s = xz_dec_init(XZ_DYNALLOC, 1u << 26);
        if (!s) { puts("\nOut of memory"); return 3; }
        b.in = inb; b.in_pos = b.in_size = 0; b.out = outb; b.out_pos = 0; b.out_size = OUTB;
        while (r == XZ_OK) {
            if (b.in_pos == b.in_size && xl) {
                unsigned k = xl < INB ? xl : INB;
                if (fread(inb, 1, k, f) != k) { printf("\nRead error in %s: bad floppy?\n", name); return 2; }
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
            printf("\nCRC/data error in %s: bad floppy, make this disk again\n", name); return 2; }
        total += size;
        printf("  %-46s %8u ok\n", name, size);
    }
    fclose(f);
    printf("Disk %u ok, %u bytes\n", h[8], total);
    return h[8] == h[9] ? 10 : 0;   /* 10 = that was the last disk */
}
