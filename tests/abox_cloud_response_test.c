#include "abox_cloud_response.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    char output[256], tiny[12];
    assert(ABoxCloudResponse_Encode(output, sizeof(output), "2.0", "9007199254740993",
        "product", "r\"1", 200U, "ok\n", "{\"saved\":true}"));
    assert(strcmp(output, "{\"version\":\"2.0\",\"timestamp\":9007199254740993,\"data\":{\"product\":\"product\",\"requestId\":\"r\\\"1\",\"code\":200,\"msg\":\"ok\\n\",\"result\":{\"saved\":true}}}") == 0);
    assert(ABoxCloudResponse_Encode(output, sizeof(output), "2.0", "0", 0, "r", 500U, "error", 0));
    assert(!strstr(output, "product"));
    assert(!ABoxCloudResponse_Encode(tiny, sizeof(tiny), "2.0", "0", 0, "r", 200U, "ok", 0));
    assert(tiny[0] == 0);
    assert(!ABoxCloudResponse_Encode(output, sizeof(output), "2.0", "1e9", 0, "r", 200U, "ok", 0));
    assert(!ABoxCloudResponse_Encode(output, sizeof(output), "2.0", "", 0, "r", 200U, "ok", 0));
    return 0;
}
