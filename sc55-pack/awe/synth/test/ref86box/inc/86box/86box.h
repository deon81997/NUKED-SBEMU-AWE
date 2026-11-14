#pragma once
#include <stdint.h>
#include <stdio.h>
void fatal(const char *fmt, ...);
void pclog(const char *fmt, ...);
void pclog_ex(const char *fmt, va_list ap);
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
