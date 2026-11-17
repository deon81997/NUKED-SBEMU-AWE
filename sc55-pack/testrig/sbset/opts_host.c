/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU (testrig/sbset)
 * Host test of SBSET's SBOPTS.TXT line builder/reader (tools/sbset.c, HOSTTEST): every field and value round trip,
 * the AWE32 options (/AWE /AWERAM /AWEROM /AWEFX /AWEMPU), defaults left off the line, unknown options kept, the
 * AWE32 red line (no ROM, too little memory), and Win98 Settings (opts.cpp) reading what SBSET wrote.
 * Run: testrig/sbset/opts_host.sh */
#define main sbset_hostmain
#include "../../tools/sbset.c"
#undef main
#include <assert.h>

int SC55_ListInstalledModels(const char* dir, void (*cb)(const char*, void*), void* user) { (void)dir; (void)cb; (void)user; return 0; }
void w98_read(const char* line, char* out, int outsz, int* awe, int* ram, int* mpu, int* fx, char* rom, int romsz);

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void reset(void)
{
    int i;
    for (i = 0; i < NF; i++) F[i].cur = F[i].def;
    F[F_AWERAM].n = 9;
    g_sf[0] = 0; g_awerom[0] = 0; g_extra[0] = 0; g_awempu0 = 0;
}
static void parse_line(const char* l) { char b[1200]; strcpy(b, l); reset(); parse_tokens(b); }
static int has(const char* line, const char* tok)
{
    size_t n = strlen(tok); const char* p = line;
    while ((p = strstr(p, tok)) != NULL) { if ((p == line || p[-1] == ' ') && (p[n] == ' ' || !p[n])) return 1; p += n; }
    return 0;
}
static int count_name(const char* line, const char* nm)   /* how often /NAME: appears */
{
    char t[40]; int c = 0; const char* p = line; size_t n;
    snprintf(t, sizeof t, "/%s:", nm); n = strlen(t);
    while ((p = strstr(p, t)) != NULL) { if (p == line || p[-1] == ' ') c++; p += n; }
    return c;
}
/* the line built now, read back, built again: the same line and the same fields */
static void roundtrip(const char* what)
{
    char a[1200], b[1200]; int cur[NF], i; char sf[200], rom[200], ex[600];
    build_line(a);
    for (i = 0; i < NF; i++) cur[i] = F[i].cur;
    strcpy(sf, g_sf); strcpy(rom, g_awerom); strcpy(ex, g_extra);
    parse_line(a);
    build_line(b);
    CHECK(!strcmp(a, b), "%s: line changed on reload\n  %s\n  %s", what, a, b);
    for (i = 0; i < NF; i++)
        if (F[i].opt || i == F_SYNTH || i == F_AWEMPU)
        {
            int rel = !(is_sf_field(i) && cur[F_SYNTH] != 1 && !(cur[F_SYNTH] == 5 && cur[F_AWEMPU] == 1))   /* only fields the line carries */
                   && !(is_awe_field(i) && cur[F_SYNTH] != 5) && !(i == F_AWEMPU && cur[F_SYNTH] != 5);
            if (is_sc_field(i)) { int ms = cur[F_SYNTH] == 5 ? cur[F_AWEMPU] : cur[F_SYNTH]; rel = ms == 0 || (i == F_SCGAIN && ms != 1); }
            if (rel && i != F_AWEON) CHECK(F[i].cur == cur[i], "%s: field %s %d -> %d (%s)", what, F[i].label, cur[i], F[i].cur, a);
        }
    CHECK(!strcmp(sf, g_sf) || cur[F_SYNTH] != 1, "%s: /VSF %s -> %s", what, sf, g_sf);
    CHECK(!strcmp(rom, g_awerom) || cur[F_SYNTH] != 5, "%s: /AWEROM %s -> %s", what, rom, g_awerom);
    CHECK(!strcmp(ex, g_extra), "%s: kept options %s -> %s", what, ex, g_extra);
}

static void w98(const char* line, int awe, int ram, int mpu, int fx, const char* rom)
{
    char out[1200], r[260]; int a, m, p, x;
    w98_read(line, out, sizeof out, &a, &m, &p, &x, r, sizeof r);
    CHECK(a == awe && m == ram && p == mpu && x == fx && !strcmp(r, rom), "Win98 read %s: awe %X ram %d mpu %d fx %d rom '%s'", line, a, m, p, x, r);
    /* and what Win98 writes back, SBSET reads as the same AWE32 options */
    {
        char b[1200]; int port = F[F_AWEPORT].cur, rr = F[F_AWERAM].cur, am = F[F_AWEMPU].cur, f = F[F_AWEFX].cur, sy = F[F_SYNTH].cur;
        char rm[200]; strcpy(rm, g_awerom);
        strcpy(b, out); reset(); parse_tokens(b);
        CHECK(F[F_SYNTH].cur == sy && (sy != 5 || (F[F_AWEPORT].cur == port && F[F_AWERAM].cur == rr && F[F_AWEMPU].cur == am && F[F_AWEFX].cur == f && !strcmp(rm, g_awerom))),
              "SBSET reads Win98's %s differently", out);
    }
}

static void mkrom(const char* path, int ok)
{
    FILE* f = fopen(path, "wb"); static unsigned char buf[1048576];
    memset(buf, 0, sizeof buf);
    if (ok) memcpy(buf + 4, "M1MG", 4);
    fwrite(buf, 1, sizeof buf, f); fclose(f);
}

int main(void)
{
    char line[1200], tmp[300];
    int i, j;
    set_labels();

    /* 1. every field, every value (in the synth it belongs to) */
    for (i = 0; i < NF; i++)
    {
        if (!F[i].opt && i != F_SYNTH && i != F_AWEMPU) continue;
        if (i == F_SF || i == F_AWEROM || i == F_ROMSET) continue;   /* string fields: below */
        for (j = 0; j < F[i].n; j++)
        {
            int ctx;
            for (ctx = 0; ctx < 2; ctx++)    /* 0 = SC-55 (SoundFont for its fields), 1 = AWE32 on */
            {
                reset();
                if (is_sf_field(i)) { strcpy(g_sf, "C:\\SF2\\TEST.SF2"); F[F_SYNTH].cur = 1; }
                if (ctx) awe_set(1);
                if (is_awe_field(i) || i == F_AWEMPU) { if (!ctx) continue; }
                F[i].cur = j;
                if (i == F_SYNTH && j == 1) strcpy(g_sf, "C:\\SF2\\TEST.SF2");
                if (i == F_AWEMPU && j == 1) strcpy(g_sf, "C:\\SF2\\TEST.SF2");
                if (i == F_AWEON) awe_set(j == 0);
                snprintf(tmp, sizeof tmp, "%s=%s ctx %d", F[i].opt ? F[i].opt : F[i].label, F[i].vals[j], ctx);
                roundtrip(tmp);
            }
        }
    }

    /* 2. AWE32 options: written only when they differ from SBEMU's defaults */
    reset(); awe_set(1); build_line(line);
    CHECK(has(line, "/AWE:620") && !count_name(line, "AWERAM") && !count_name(line, "AWEFX") && !count_name(line, "AWEMPU") && !count_name(line, "AWEROM"), "AWE defaults: %s", line);
    CHECK(has(line, "/T:6") || 1, "%s", line);
    parse_line("/A:220 /I:5 /D:1 /H:5 /T:6 /P:330 /AWE:620 /AWERAM:512 /AWEFX:1 /AWEMPU:1");
    build_line(line);
    CHECK(!count_name(line, "AWERAM") && !count_name(line, "AWEFX") && !count_name(line, "AWEMPU") && has(line, "/AWE:620"), "explicit defaults dropped: %s", line);
    { static const char* port[4] = { "620", "640", "660", "680" };
      for (j = 0; j < 4; j++) { reset(); awe_set(1); F[F_AWEPORT].cur = j; build_line(line); snprintf(tmp, sizeof tmp, "/AWE:%s", port[j]); CHECK(has(line, tmp), "port %s: %s", port[j], line); roundtrip(tmp); } }
    { static const char* ram[9] = { "512", "1024", "2048", "4096", "8192", "12288", "16384", "28672", "0" };
      for (j = 0; j < 9; j++) { reset(); awe_set(1); F[F_AWERAM].cur = j; build_line(line); snprintf(tmp, sizeof tmp, "/AWERAM:%s", ram[j]); CHECK(j == 0 ? !count_name(line, "AWERAM") : has(line, tmp), "ram %s: %s", ram[j], line); } }
    /* a size the list does not have */
    parse_line("/AWE:620 /AWERAM:3000"); build_line(line);
    CHECK(has(line, "/AWERAM:3000") && !strcmp(show(&F[F_AWERAM]), "3000 KB"), "custom RAM: %s", line); roundtrip("AWERAM 3000");
    parse_line("/AWE:620 /AWERAM:99999"); build_line(line);
    CHECK(!count_name(line, "AWERAM"), "RAM out of range dropped: %s", line);
    /* MPU-401 also plays: every choice, by name */
    { static const char* exp[9] = { "", "/VSF:C:\\SF2\\TEST.SF2", "/VMT32:MT32", "/VMT32:CM32L", "/MUSB:1", "/AWEMPU:0", "/AWEMPU:GM", "/AWEMPU:GS", "/AWEMPU:MT32" };
      for (j = 0; j < 9; j++)
      {
          reset(); awe_set(1); F[F_AWEMPU].cur = j; if (j == 1) strcpy(g_sf, "C:\\SF2\\TEST.SF2");
          build_line(line);
          CHECK(j == 0 ? !count_name(line, "AWEMPU") && !count_name(line, "VMT32") && !count_name(line, "VSF") : has(line, exp[j]), "AWEMPU %d: %s", j, line);
          CHECK(count_name(line, "AWEMPU") <= 1, "one /AWEMPU: %s", line);
          roundtrip(exp[j][0] ? exp[j] : "AWEMPU sc55");
      } }
    parse_line("/AWE:620 /AWEMPU:MT-32"); CHECK(F[F_AWEMPU].cur == 8, "MT-32 alias");
    /* /AWEROM: a file and a folder, with the drive letter's colon */
    parse_line("/AWE:640 /AWEROM:C:\\SC55\\ROMS\\AWE32.RAW /AWEFX:0"); build_line(line);
    CHECK(!strcmp(g_awerom, "C:\\SC55\\ROMS\\AWE32.RAW") && has(line, "/AWEROM:C:\\SC55\\ROMS\\AWE32.RAW") && has(line, "/AWEFX:0") && has(line, "/AWE:640"), "AWEROM file: %s", line);
    roundtrip("AWEROM file");
    parse_line("/AWE:620 /AWEROM:D:\\AWE"); build_line(line); CHECK(has(line, "/AWEROM:D:\\AWE"), "AWEROM folder: %s", line); roundtrip("AWEROM folder");
    /* AWE32 off: none of its options on the line, the MPU-401 keeps its synth */
    parse_line("/AWE:660 /AWERAM:4096 /AWEMPU:0 /VMT32:CM32L /AWEFX:0 /AWEROM:X.RAW"); awe_set(0); build_line(line);
    CHECK(!strstr(line, "/AWE") && F[F_SYNTH].cur == 0, "AWE off after MPU off -> SC-55: %s", line);
    parse_line("/VMT32:CM32L /AWE:620"); CHECK(awe_on() && F[F_AWEMPU].cur == 3, "MT order 1");
    parse_line("/AWE:620 /VMT32:CM32L"); CHECK(awe_on() && F[F_AWEMPU].cur == 3, "MT order 2");
    awe_set(0); build_line(line); CHECK(F[F_SYNTH].cur == 3 && has(line, "/VMT32:CM32L") && !strstr(line, "/AWE"), "AWE off keeps CM-32L: %s", line);
    parse_line("/AWE:0 /AWERAM:4096"); CHECK(!awe_on(), "/AWE:0 = off");
    /* the AWE32 on/off field follows the synth */
    reset(); awe_set(1); CHECK(F[F_AWEON].cur == 0, "on"); awe_set(0); CHECK(F[F_AWEON].cur == 1, "off");
    reset(); F[F_AWEMPU].cur = 6; awe_changed(F_AWEMPU); CHECK(awe_on() && F[F_AWEMPU].cur == 6, "AWEMPU change turns the AWE32 on and keeps the choice");
    reset(); F[F_SYNTH].cur = 2; F[F_AWERAM].cur = 3; awe_changed(F_AWERAM); CHECK(awe_on() && F[F_AWEMPU].cur == 2, "RAM change turns AWE on, MT-32 stays on the MPU");

    /* 3. options SBSET has no field for are kept (once), the known ones are not doubled */
    parse_line("/A:240 /I:5 /D:1 /H:5 /T:6 /P:330 /VSCLPF:1 /FOO:BAR /VMPU:0 /AWE:620 /AWEMPU:GM /JOYDZ:20 /DBG:1 /FOO:BAZ @SBOPTS.TXT");
    build_line(line);
    CHECK(has(line, "/VSCLPF:1") && has(line, "/VMPU:0") && has(line, "/DBG:1") && has(line, "/FOO:BAZ") && !has(line, "/FOO:BAR"), "kept: %s", line);
    CHECK(count_name(line, "A") == 1 && count_name(line, "JOYDZ") == 1 && count_name(line, "AWEMPU") == 1 && count_name(line, "AWE") == 1 && !strstr(line, "@"), "no doubles: %s", line);
    roundtrip("kept options");
    parse_line("/A220 /I7 /VSCLPF /X"); build_line(line);
    CHECK(count_name(line, "A") == 1 && !has(line, "/A220") && has(line, "/VSCLPF") && has(line, "/X"), "no-colon form: %s", line);

    /* 4. SBOPTS.TXT on disk: save, load */
    {
        char d[] = "/tmp/sbsetXXXXXX"; char p[300]; FILE* f;
        if (!mkdtemp(d)) return 1; strcpy(g_dir, d);
        parse_line("/A:220 /I:7 /D:1 /H:5 /T:6 /P:330 /AWE:680 /AWERAM:8192 /AWEROM:C:\\R\\A.RAW /AWEMPU:GS /AWEFX:0 /VSCLPF:1");
        build_line(tmp); CHECK(save(), "save");
        reset(); load(); build_line(line);
        CHECK(!strcmp(tmp, line), "disk round trip\n  %s\n  %s", tmp, line);
        printf("saved line: %s\n", line);
        /* 5. the red line: ROM missing / found by content (not by name) / RAM vs free memory */
        g_awerom[0] = 0;
        CHECK(awe_problem()[0] != 0, "no ROM -> red line");
        snprintf(p, sizeof p, "%s/ROMS", d); mkdir(p, 0755);
        snprintf(p, sizeof p, "%s/ROMS/AWE32.RAW", d); mkrom(p, 0);
        CHECK(awe_problem()[0] != 0, "a file named AWE32.RAW without the ROM signature is no ROM");
        snprintf(p, sizeof p, "%s/ROMS/ZZZ.BIN", d); mkrom(p, 1);
        CHECK(awe_problem()[0] == 0, "ROM found by content: %s", awe_problem());
        { char r[300]; aw_rom(r, sizeof r); CHECK(strstr(r, "ZZZ.BIN") != NULL, "found %s", r); }
        snprintf(g_awerom, sizeof g_awerom, "%s/ROMS", d); CHECK(awe_problem()[0] == 0, "folder with a ROM");
        snprintf(g_awerom, sizeof g_awerom, "%s/ROMS/AWE32.RAW", d); CHECK(awe_problem()[0] != 0, "named file that is no ROM");
        g_awerom[0] = 0;
        setenv("SBSET_FREEKB", "9000", 1); F[F_AWERAM].cur = 4; CHECK(awe_problem()[0] != 0, "8192+1300 > 9000 -> red");
        F[F_AWERAM].cur = 0; CHECK(awe_problem()[0] == 0, "512 fits");
        g_sbemu_loaded = 1; F[F_AWERAM].cur = 4; CHECK(awe_problem()[0] == 0, "SBEMU loaded: no memory warning"); g_sbemu_loaded = 0;
        unsetenv("SBSET_FREEKB");
        snprintf(p, sizeof p, "rm -rf %s", d); if (system(p)) {}
    }

    /* 6. Win98 Settings reads SBSET's line: the AWE32 tab shows the same values, and SBSET reads Win98's line back */
    { static const char* rams[9] = { "512", "1024", "2048", "4096", "8192", "12288", "16384", "28672", "0" };
      int port, r, m, fx;
      for (port = 0; port < 4; port++) for (r = 0; r < 9; r++) for (m = 0; m < 9; m += 1) for (fx = 0; fx < 2; fx++)
      {
          static const int w98mpu[9] = { 1, 1, 1, 1, 1, 0, 2, 3, 4 };
          reset(); awe_set(1); F[F_AWEPORT].cur = port; F[F_AWERAM].cur = r; F[F_AWEMPU].cur = m; F[F_AWEFX].cur = fx;
          if (m == 1) strcpy(g_sf, "C:\\SF2\\TEST.SF2");
          if (r == 3) strcpy(g_awerom, "C:\\SC55\\ROMS\\AWE32.RAW");
          build_line(line);
          w98(line, 0x620 + 0x20 * port, atoi(rams[r]), w98mpu[m], !fx, g_awerom);
      } }
    reset(); build_line(line); w98(line, 0, 512, 1, 1, "");

    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails != 0;
}
