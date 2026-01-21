#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

void timestamp_update(char *buf, size_t sz)
{
    time_t now = time(NULL);
    struct tm tm_utc;
#if defined(_WIN32)
    gmtime_s(&tm_utc, &now);
#else
    gmtime_r(&now, &tm_utc);
#endif
    strftime(buf, sz, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

int append_line_to_file(const char *path, const char *line)
{
    FILE *f = fopen(path, "a");
    if (!f) {
        return -1;
    }
    if (fputs(line, f) == EOF) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int run_cmd_capture(const char *cmd, char *out, size_t outsz)
{
    if (!cmd) return -1;
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    size_t n = 0;
    if (out && outsz) {
        n = fread(out, 1, outsz - 1, p);
        out[n] = '\0';
    }
    (void)pclose(p);
    return 0;
}
