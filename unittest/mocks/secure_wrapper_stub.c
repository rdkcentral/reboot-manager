#include <stdarg.h>
#include <stdio.h>

/* Minimal stub for secure wrapper used in unit tests */
int v_secure_system(const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt ? fmt : "", ap);
    va_end(ap);
    return 0;
}
