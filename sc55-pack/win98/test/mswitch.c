/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* mswitch.c - w98.8 test: switches SC55W's MIDI output while a DOS game plays (SC55W /MOUT:x run again).
 * MSWITCH [count] [seconds] [first wait] [what]   default 10 times, every 15 s, after 60 s, NEXT. Log C:\MSWITCH.LOG */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR cmd, int s)
{
    int n = 10, sec = 15, first = 60, i;
    char what[64] = "NEXT", line[200];
    FILE *f;
    (void)a; (void)b; (void)s;
    {   /* no sscanf: MinGW's pulls in _strtoui64, which Windows 98's MSVCRT.DLL does not have */
        char *p = cmd; int *v[3] = {&n, &sec, &first}, k;
        for (k = 0; k < 3 && *p; k++) { while (*p == ' ') p++; if (*p >= '0' && *p <= '9') *v[k] = (int)strtol(p, &p, 10); }
        while (*p == ' ') p++;
        if (*p) { lstrcpynA(what, p, sizeof what); }
    }
    Sleep((DWORD)first * 1000);
    for (i = 0; i < n; i++) {
        snprintf(line, sizeof line, "C:\\SC55\\WIN98\\SC55W.EXE /MOUT:%s", what);
        WinExec(line, SW_HIDE);
        f = fopen("C:\\MSWITCH.LOG", "a");
        if (f) { fprintf(f, "%lu switch %d\n", GetTickCount(), i + 1); fclose(f); }
        Sleep((DWORD)sec * 1000);
    }
    return 0;
}
