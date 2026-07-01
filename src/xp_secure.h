#pragma once
/* XP: msvcrt.dll antigo nao exporta as funcoes seguras *_s.
   Este header (force-included so no build XP) as substitui por versoes
   inline proprias. Incluir os headers de sistema ANTES de definir as macros
   garante que os prototipos reais sejam parseados sem serem mangleados. */
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static __inline int xp_snwprintf_s(wchar_t *d, size_t dn, size_t count,
                                   const wchar_t *fmt, ...) {
    (void)count;
    va_list ap; va_start(ap, fmt);
    int r = _vsnwprintf(d, dn, fmt, ap);
    va_end(ap);
    if (dn) d[dn - 1] = 0;   /* _vsnwprintf nao termina se truncar */
    return r;
}
static __inline int xp_wcsncpy_s(wchar_t *d, size_t dn,
                                 const wchar_t *s, size_t count) {
    (void)count;
    if (!dn) return 0;
    size_t i = 0;
    for (; i < dn - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
    return 0;
}
static __inline int xp_wcscpy_s(wchar_t *d, size_t dn, const wchar_t *s) {
    return xp_wcsncpy_s(d, dn, s, dn);
}
static __inline int xp_wcsncat_s(wchar_t *d, size_t dn,
                                 const wchar_t *s, size_t count) {
    (void)count;
    size_t dl = 0;
    while (dl < dn && d[dl]) dl++;        /* comprimento atual, limitado */
    if (dl >= dn) return 0;               /* sem espaco / nao terminado */
    size_t i = 0;
    for (; dl + i < dn - 1 && s[i]; i++) d[dl + i] = s[i];
    d[dl + i] = 0;
    return 0;
}

#define _snwprintf_s xp_snwprintf_s
#define wcsncpy_s    xp_wcsncpy_s
#define wcscpy_s     xp_wcscpy_s
#define wcsncat_s    xp_wcsncat_s
