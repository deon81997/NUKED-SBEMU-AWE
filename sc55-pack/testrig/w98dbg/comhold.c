/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* COMHOLD.EXE COM1 SECS OUT.TXT - w98.9 debug link test: open the port as a Windows program would, write the result
 * ("open ok" / "open failed <error>") to OUT.TXT, keep it open SECS seconds (0 = close at once). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR cmd, int n)
{
    char port[16] = "COM1", out[260] = "C:\\COMHOLD.TXT"; int secs = 0;
    (void)a; (void)b; (void)n;
    char* t = strtok(cmd, " ");                      /* no sscanf: MinGW's pulls _strtoui64 (not in Win98 MSVCRT) */
    if (t) { lstrcpynA(port, t, sizeof port); t = strtok(NULL, " "); }
    if (t) { secs = atoi(t); t = strtok(NULL, " "); }
    if (t) lstrcpynA(out, t, sizeof out);
    HANDLE h = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    FILE* f = fopen(out, "a");
    if (f) {
        if (h == INVALID_HANDLE_VALUE) fprintf(f, "%s open failed %lu\r\n", port, GetLastError());
        else fprintf(f, "%s open ok\r\n", port);
        fclose(f);
    }
    if (h != INVALID_HANDLE_VALUE) { Sleep((DWORD)secs * 1000); CloseHandle(h); }
    return h == INVALID_HANDLE_VALUE;
}
