#include "abox_boot_v2_app.h"
#include "abox_platform_port.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASE 0x08000000U
#define STATE_A 0x0803E800U
#define STATE_B 0x0803F000U

static uint8_t flash_mem[0x40000];
static uint8_t transfer[ABOX_BOOT_V2_APP_RAW_CHUNK];
static ABoxBootV2AtDoneFn pending_done;
static void *pending_user;
static ABoxBootV2AtEventFn event_fn;
static void *event_user;
static char command[300];
static uint8_t pending;
static uint8_t active;
static uint8_t mqtt_paused;
static uint32_t payload_count;
static uint32_t raw_wanted;

static int flash_begin(void *context) { (void)context; return 1; }
static void flash_end(void *context) { (void)context; }
static int flash_read(void *context, uint32_t address, uint8_t *data, uint32_t length)
{
    (void)context;
    if (address < BASE || address - BASE + length > sizeof(flash_mem)) return 0;
    memcpy(data, flash_mem + address - BASE, length);
    return 1;
}
static int flash_write(void *context, uint32_t address, const uint8_t *data, uint32_t length)
{
    (void)context;
    if (address < BASE || address - BASE + length > sizeof(flash_mem)) return 0;
    memcpy(flash_mem + address - BASE, data, length);
    return 1;
}
static int flash_erase(void *context, uint32_t address)
{
    (void)context;
    if (address < BASE || address - BASE + 0x800U > sizeof(flash_mem)) return 0;
    memset(flash_mem + address - BASE, 0xFF, 0x800U);
    return 1;
}
static uint32_t tick(void *context) { (void)context; return 1000U; }
static int uart(void *context, const uint8_t *data, uint32_t length)
{ (void)context; (void)data; return length != 0U; }
static void critical(void *context) { (void)context; }

static int submit(void *context, const char *value, uint32_t timeout,
                  ABoxBootV2AtDoneFn done, void *user)
{
    (void)context; (void)timeout;
    assert(!pending);
    snprintf(command, sizeof(command), "%s", value);
    pending_done = done; pending_user = user; pending = 1U; active = 1U;
    return 1;
}
static int send_payload(void *context, const uint8_t *data, uint16_t length)
{ (void)context; assert(data && length); payload_count++; return 1; }
static int begin_raw(void *context, uint32_t length)
{ (void)context; raw_wanted = length; return 1; }
static int has_pending(void *context) { (void)context; return pending; }
static int is_active(void *context) { (void)context; return active; }
static void cancel(void *context) { (void)context; pending = 0U; active = 0U; }
static void register_events(void *context, ABoxBootV2AtEventFn callback, void *user)
{ (void)context; event_fn = callback; event_user = user; }
static void mqtt_pause(void *context, uint8_t paused) { (void)context; mqtt_paused = paused; }
static void log_line(void *context, uint8_t level, const char *message)
{ (void)context; (void)level; (void)message; }

static void complete(ABoxBootV2AtResult result)
{
    ABoxBootV2AtDoneFn callback = pending_done;
    void *user = pending_user;
    assert(pending && callback);
    pending = 0U; active = 0U; pending_done = 0; pending_user = 0;
    callback(result, user);
}
static void line(const char *value)
{ assert(event_fn); event_fn(ABOX_BOOT_V2_AT_LINE, (const uint8_t *)value, (uint16_t)strlen(value), event_user); }
static void raw(const uint8_t *data, uint16_t length)
{ assert(event_fn); event_fn(ABOX_BOOT_V2_AT_RAW, data, length, event_user); }

int main(void)
{
    static const ABoxFlashLayout legacy = {0x08008000U, 0x0803F800U, 0x800U};
    static const ABoxPlatformPort platform = {
        0, tick, uart, flash_begin, flash_write, flash_erase, flash_end,
        critical, critical, 0, 0, &legacy, 0, 0, flash_read
    };
    static const ABoxBootV2Layout layout = {
        STATE_A, STATE_B, 0x800U, 0x08008000U, 0x0803E7FFU, 0x20000000U, 0x2000FFFFU
    };
    static const uint8_t ca[] = "test-ca";
    ABoxBootV2AppPort port = {
        0, tick, submit, send_payload, begin_raw, has_pending, is_active,
        cancel, register_events, mqtt_pause, log_line, transfer, sizeof(transfer)
    };
    ABoxBootV2AppConfig config = {
        "https://ota.example:20443", "meal_delivery_vehicle", "meal_delivery_vehicle_app.bin",
        "abox-v2-", "abox-v2-current", ca, sizeof(ca) - 1U,
        0x08008000U, 128U, 0x36800U
    };
    ABoxBootV2Record state;
    ABoxBootV2Descriptor descriptor;
    ABoxBootV2AppRequest request;
    uint8_t image[128];
    uint32_t index;

    memset(flash_mem, 0xFF, sizeof(flash_mem));
    assert(ABox_PlatformPortBind(&platform));
    assert(ABoxBootV2_StateBindLayout(&layout));
    memset(&state, 0, sizeof(state));
    state.state = ABOX_BOOT_V2_CONFIRMED;
    state.stable_slot = 0U;
    state.stable_image_size = 64U;
    state.stable_image_crc32 = 0x11111111U;
    memcpy(state.stable_image_version, "abox-v2-stable", 15U);
    assert(ABoxBootV2_StateSave(&state));
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.magic = ABOX_BOOT_V2_DESCRIPTOR_MAGIC;
    descriptor.abi_version = 1U;
    descriptor.length = sizeof(descriptor);
    descriptor.feature_flags = ABOX_BOOT_V2_REQUIRED_FEATURES;
    descriptor.crc32 = ABoxBootV2_Crc32(&descriptor, (uint32_t)offsetof(ABoxBootV2Descriptor, crc32));
    memcpy(flash_mem + ABOX_BOOT_V2_DESCRIPTOR_ADDR - BASE, &descriptor, sizeof(descriptor));

    assert(ABoxBootV2App_Init(&port, &config));
    assert(ABoxBootV2App_BeginProvisioning());
    for (index = 0U; index < 11U; ++index) complete(ABOX_BOOT_V2_AT_OK);
    assert(strstr(command, "QFDEL") != 0);
    complete(ABOX_BOOT_V2_AT_ERROR);
    assert(strstr(command, "QFUPL") != 0);
    ABoxBootV2App_Task();
    assert(payload_count == 1U);
    complete(ABOX_BOOT_V2_AT_OK);
    assert(ABoxBootV2App_IsProvisioned());

    memset(&request, 0, sizeof(request));
    snprintf(request.url, sizeof(request.url), "https://wrong.example/fw.bin");
    snprintf(request.version, sizeof(request.version), "abox-v2-next");
    request.size = sizeof(image); request.crc32 = 1U;
    assert(!ABoxBootV2App_Start(&request));
    assert(ABoxBootV2App_LastError() == ABOX_BOOT_V2_APP_ERROR_URL);

    memset(image, 0xA5, sizeof(image));
    image[0] = 0x00U; image[1] = 0x40U; image[2] = 0x00U; image[3] = 0x20U;
    image[4] = 0x01U; image[5] = 0x81U; image[6] = 0x00U; image[7] = 0x08U;
    snprintf(request.url, sizeof(request.url),
             "https://ota.example:20443/fw-revisions/meal_delivery_vehicle/abox-v2-next/meal_delivery_vehicle_app.bin");
    request.crc32 = ABoxBootV2_Crc32(image, sizeof(image));
    assert(ABoxBootV2App_Start(&request));
    assert(mqtt_paused);
    for (index = 0U; index < 6U; ++index) complete(ABOX_BOOT_V2_AT_OK);
    for (index = 0U; index < 5U; ++index) {
        ABoxBootV2App_Task();
        complete(ABOX_BOOT_V2_AT_OK);
    }
    ABoxBootV2App_Task();
    assert(strstr(command, "QHTTPURL") != 0);
    ABoxBootV2App_Task();
    assert(payload_count == 2U);
    complete(ABOX_BOOT_V2_AT_OK);
    complete(ABOX_BOOT_V2_AT_ERROR);
    line("+QFOPEN: 7"); complete(ABOX_BOOT_V2_AT_OK);
    ABoxBootV2App_Task();
    assert(strstr(command, "QHTTPGETEX=80,0,128") != 0);
    line("+QHTTPGET: 0,206,128"); complete(ABOX_BOOT_V2_AT_OK);
    line("CONNECT"); assert(raw_wanted == sizeof(image)); raw(image, sizeof(image)); complete(ABOX_BOOT_V2_AT_OK);
    ABoxBootV2App_Task(); assert(payload_count == 3U);
    line("+QFWRITE: 128,128"); complete(ABOX_BOOT_V2_AT_OK);
    complete(ABOX_BOOT_V2_AT_OK);
    line("+QFLST: \"fw_slot1.bin\",128"); complete(ABOX_BOOT_V2_AT_OK);
    line("+QFOPEN: 8"); complete(ABOX_BOOT_V2_AT_OK);
    ABoxBootV2App_Task(); assert(strstr(command, "QFREAD=8,128") != 0);
    line("CONNECT 128"); assert(raw_wanted == sizeof(image)); raw(image, sizeof(image)); complete(ABOX_BOOT_V2_AT_OK);
    complete(ABOX_BOOT_V2_AT_OK);
    ABoxBootV2App_Task();
    assert(ABoxBootV2App_TakeInstallReady());
    assert(!mqtt_paused);
    assert(ABoxBootV2_StateLoad(&state));
    assert(state.state == ABOX_BOOT_V2_INSTALL_PENDING);
    assert(state.candidate_slot == 1U);
    assert(state.image_size == sizeof(image));
    assert(state.image_crc32 == request.crc32);
    assert(strcmp(state.image_version, "abox-v2-next") == 0);
    return 0;
}
