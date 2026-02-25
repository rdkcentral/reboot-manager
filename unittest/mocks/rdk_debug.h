#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <string.h>

/* Define log level constants; use int for compatibility in tests */
#ifndef RDK_LOG_DEBUG
#define RDK_LOG_DEBUG 0
#endif
#ifndef RDK_LOG_INFO
#define RDK_LOG_INFO 1
#endif
#ifndef RDK_LOG_WARN
#define RDK_LOG_WARN 2
#endif
#ifndef RDK_LOG_ERROR
#define RDK_LOG_ERROR 3
#endif

typedef enum { RDKLOG_OUTPUT_CONSOLE=0 } rdklog_output_t;
typedef enum { RDKLOG_FORMAT_WITH_TS=0 } rdklog_format_t;

typedef struct {
    const char* pModuleName;
    int loglevel;
    rdklog_output_t output;
    rdklog_format_t format;
    void* pFilePolicy;
} rdk_logger_ext_config_t;

#define RDK_SUCCESS 0

static inline int rdk_logger_ext_init(rdk_logger_ext_config_t* cfg){ (void)cfg; return RDK_SUCCESS; }
static inline int rdk_logger_init(const char* path){ (void)path; return 0; }

#if defined(__GNUC__)
#define RDK_LOG(level, module, fmt, ...) \
    do { (void)level; (void)module; fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)
#else
#define RDK_LOG(level, module, ...) \
    do { (void)level; (void)module; fprintf(stderr, __VA_ARGS__); } while(0)
#endif

#ifdef __cplusplus
}
#endif
