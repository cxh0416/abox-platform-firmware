#ifndef ABOX_CLOUD_RESPONSE_H
#define ABOX_CLOUD_RESPONSE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Decimal timestamp and result are already encoded by the caller. No heap,
 * floating point conversion or product identity is supplied by Platform. */
int ABoxCloudResponse_Encode(char *output, size_t capacity,
    const char *version, const char *timestamp, const char *product,
    const char *request_id, uint16_t code, const char *message,
    const char *result_json);
#ifdef __cplusplus
}
#endif
#endif
