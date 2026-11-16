/* SPDX-License-Identifier: GPL-2.0-or-later - part of NUKED SB-EMU, https://github.com/deon81997/NUKED-SBEMU-MUXED */
/* w32shim/windows.h - just enough Win32 for native (Linux) unit tests of the helper (routetest.cpp) */
#pragma once
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
typedef uint32_t DWORD, UINT, MMRESULT; typedef int32_t LONG; typedef int BOOL; typedef void* HANDLE; typedef void* LPVOID;
typedef uintptr_t DWORD_PTR, UINT_PTR; typedef char* LPSTR; typedef uint16_t WORD;
#define WINAPI
#define FALSE 0
#define TRUE 1
#define INFINITE 0xFFFFFFFFu
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_PRIORITY_ABOVE_NORMAL 1
static inline LONG InterlockedExchange(volatile LONG* p, LONG v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
static inline LONG InterlockedIncrement(volatile LONG* p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
static inline void Sleep(DWORD ms) { usleep(ms * 1000); }
static inline DWORD GetTickCount(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (DWORD)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
struct ShimEv { pthread_mutex_t m; pthread_cond_t c; int set; };
static inline HANDLE CreateEventA(void*, BOOL, BOOL, const char*) { ShimEv* e = new ShimEv; pthread_mutex_init(&e->m, 0); pthread_cond_init(&e->c, 0); e->set = 0; return e; }
static inline BOOL SetEvent(HANDLE h) { ShimEv* e = (ShimEv*)h; pthread_mutex_lock(&e->m); e->set = 1; pthread_cond_signal(&e->c); pthread_mutex_unlock(&e->m); return 1; }
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
struct ShimTh { pthread_t t; LPTHREAD_START_ROUTINE f; LPVOID a; int th; };
static void* shim_tmain(void* p) { ShimTh* t = (ShimTh*)p; t->f(t->a); return 0; }
static inline HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE f, LPVOID a, DWORD, DWORD* id) { ShimTh* t = new ShimTh; t->f = f; t->a = a; t->th = 1; pthread_create(&t->t, 0, shim_tmain, t); if (id) *id = 1; return t; }
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
    if (((ShimTh*)h)->th == 1 && ms >= 1000) { pthread_join(((ShimTh*)h)->t, 0); ((ShimTh*)h)->th = 2; return 0; }
    ShimEv* e = (ShimEv*)h; struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += (long)(ms % 1000) * 1000000; ts.tv_sec += ms / 1000 + ts.tv_nsec / 1000000000; ts.tv_nsec %= 1000000000;
    pthread_mutex_lock(&e->m); while (!e->set) if (pthread_cond_timedwait(&e->c, &e->m, &ts)) break; e->set = 0; pthread_mutex_unlock(&e->m); return 0;
}
static inline BOOL CloseHandle(HANDLE) { return 1; }
static inline BOOL SetThreadPriority(HANDLE, int) { return 1; }
/* file search: none found (pad_select's folder scan) */
typedef struct { DWORD dwFileAttributes; DWORD nFileSizeLow; char cFileName[260]; char cAlternateFileName[14]; } WIN32_FIND_DATAA;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
static inline HANDLE FindFirstFileA(const char*, WIN32_FIND_DATAA*) { return INVALID_HANDLE_VALUE; }
static inline BOOL FindNextFileA(HANDLE, WIN32_FIND_DATAA*) { return 0; }
static inline BOOL FindClose(HANDLE) { return 1; }
