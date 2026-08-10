#include "abox_platform_config.h"

int main(void)
{
    const ABoxProductConfig config = {
        "test", 0x08000000U, 256U * 1024U, 0x08000000U, 0x8000U,
        0x08008000U, 0x0803F800U, 0x800U, 500000U, 500000U,
        115200U, ABOX_SCHEDULER_BAREMETAL
    };
    return ABox_ProductConfigIsValid(&config) ? 0 : 1;
}
