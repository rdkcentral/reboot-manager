/* Test stub header for telemetry bus message sender */
#ifndef TELEMETRY_BUSMESSAGE_SENDER_H
#define TELEMETRY_BUSMESSAGE_SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

void t2_event_d(const char* marker, int val);
void t2_event_s(const char* marker, const char* val);
void t2_init(const char* client);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_BUSMESSAGE_SENDER_H */
