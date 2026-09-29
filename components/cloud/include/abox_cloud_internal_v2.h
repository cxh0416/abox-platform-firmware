#ifndef ABOX_CLOUD_INTERNAL_V2_H
#define ABOX_CLOUD_INTERNAL_V2_H

#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared envelope for the legacy internal Topic V2 request contract. Products
 * still validate params, choose actions, and map errors to their own codes. */
typedef struct {
    cJSON *root;
    cJSON *params;
    const char *request_id;
    cJSON *command;
} ABoxCloudInternalV2Envelope;

int ABoxCloudRequestId_Valid(const char *value, size_t capacity);
/* Recover an unescaped requestId from malformed JSON for a product's existing
 * error response. This is deliberately bounded and does not decode escapes. */
void ABoxCloudRequestId_ExtractRaw(const char *payload, size_t length,
                                   char *output, size_t capacity);
/* expected_product=NULL preserves products that do not validate the optional
 * data.product field. The returned pointers live until Close. */
int ABoxCloudInternalV2_Open(const char *payload, size_t length,
                             const char *expected_product,
                             size_t request_id_capacity,
                             ABoxCloudInternalV2Envelope *envelope);
void ABoxCloudInternalV2_Close(ABoxCloudInternalV2Envelope *envelope);

#ifdef __cplusplus
}
#endif

#endif
