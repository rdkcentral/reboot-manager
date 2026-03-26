#include <cstdio>
#include "telemetry_busmessage_sender.h"

void t2_event_d(const char* marker, int value) {
    if (marker) {
        fprintf(stderr, "T2_D:%s:%d\n", marker, value);
    }
}

void t2_event_s(const char* marker, const char* value) {
    if (marker && value) {
        fprintf(stderr, "T2_S:%s:%s\n", marker, value);
    }
}
