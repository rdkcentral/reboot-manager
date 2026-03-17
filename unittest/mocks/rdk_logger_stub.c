#include <stdarg.h>
#include <stdio.h>

/* Minimal stub to satisfy linkage when system rdk_logger.h is included */
int rdk_logger_msg_printf(int level, const char* module, const char* fmt, ...){
    (void)level; (void)module;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    return 0;
}

/* Stubs for initialization APIs when system prototypes are visible */
int rdk_logger_ext_init(const void* config){ (void)config; return 0; }
int rdk_logger_init(const char* path){ (void)path; return 0; }
