#include "abox_cloud_internal_v2.h"

#include <string.h>

int ABoxCloudRequestId_Valid(const char *value, size_t capacity)
{
    size_t i;
    if (!value || !capacity || value[0] == '\0') return 0;
    for (i = 0U; i < capacity; ++i) {
        uint8_t ch = (uint8_t)value[i];
        if (ch == '\0') return 1;
        if (ch < 0x21U || ch > 0x7EU) return 0;
    }
    return 0;
}

void ABoxCloudRequestId_ExtractRaw(const char *payload, size_t length,
                                   char *output, size_t capacity)
{
    static const char key[] = "\"requestId\"";
    size_t i;
    if (!output || !capacity) return;
    output[0] = '\0';
    if (!payload || length < sizeof(key)) return;
    for (i = 0U; i + sizeof(key) - 1U < length; ++i) {
        size_t pos, used = 0U;
        if (memcmp(payload + i, key, sizeof(key) - 1U) != 0) continue;
        pos = i + sizeof(key) - 1U;
        while (pos < length && (payload[pos] == ' ' || payload[pos] == '\t' ||
               payload[pos] == '\r' || payload[pos] == '\n' || payload[pos] == ':')) ++pos;
        if (pos >= length || payload[pos++] != '"') return;
        while (pos < length && payload[pos] != '"' && used < capacity - 1U) {
            if (payload[pos] == '\\') {
                output[0] = '\0';
                return;
            }
            output[used++] = payload[pos++];
        }
        output[used] = '\0';
        if (!ABoxCloudRequestId_Valid(output, capacity)) output[0] = '\0';
        return;
    }
}

int ABoxCloudInternalV2_Open(const char *payload, size_t length,
                             const char *expected_product,
                             size_t request_id_capacity,
                             ABoxCloudInternalV2Envelope *envelope)
{
    cJSON *version, *data, *product, *request_id, *command, *params;
    if (!envelope) return 0;
    memset(envelope, 0, sizeof(*envelope));
    if (!payload || !request_id_capacity) return 0;
    envelope->root = cJSON_ParseWithLength(payload, length);
    if (!envelope->root) return 0;
    version = cJSON_GetObjectItemCaseSensitive(envelope->root, "version");
    data = cJSON_GetObjectItemCaseSensitive(envelope->root, "data");
    product = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "product") : 0;
    request_id = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "requestId") : 0;
    command = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "command") : 0;
    params = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "params") : 0;
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0 ||
        !cJSON_IsObject(data) || !cJSON_IsString(request_id) ||
        !ABoxCloudRequestId_Valid(request_id->valuestring, request_id_capacity) ||
        !cJSON_IsString(command) || !cJSON_IsObject(params) ||
        (expected_product && product &&
         (!cJSON_IsString(product) ||
          strcmp(product->valuestring, expected_product) != 0))) {
        ABoxCloudInternalV2_Close(envelope);
        return 0;
    }
    envelope->params = params;
    envelope->request_id = request_id->valuestring;
    envelope->command = command;
    return 1;
}

void ABoxCloudInternalV2_Close(ABoxCloudInternalV2Envelope *envelope)
{
    if (!envelope) return;
    cJSON_Delete(envelope->root);
    memset(envelope, 0, sizeof(*envelope));
}

int ABoxCloudInternalV2_PskCredentials(const cJSON *params, ABoxTlsCredentials *output)
{
    const cJSON *identity = cJSON_GetObjectItemCaseSensitive(params, "identity");
    const cJSON *secret = cJSON_GetObjectItemCaseSensitive(params, "secret");
    const cJSON *generation = cJSON_GetObjectItemCaseSensitive(params, "generation");
    memset(output, 0, sizeof(*output));
    if (!cJSON_IsString(identity) || strlen(identity->valuestring) >= sizeof(output->identity) ||
        !cJSON_IsString(secret) || strlen(secret->valuestring) != 32U ||
        !cJSON_IsNumber(generation) || generation->valuedouble < 1 ||
        generation->valuedouble > 4294967295.0 ||
        generation->valuedouble != (uint32_t)generation->valuedouble) return 0;
    output->mode = ABOX_TLS_AUTH_PSK;
    output->generation = (uint32_t)generation->valuedouble;
    strcpy(output->identity, identity->valuestring);
    strcpy(output->secret, secret->valuestring);
    if (ABoxTlsCredentials_Valid(output)) return 1;
    memset(output, 0, sizeof(*output));
    return 0;
}
