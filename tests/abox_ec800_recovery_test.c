#include "abox_ec800_recovery.h"

#include <assert.h>

int main(void)
{
    ABoxEc800Recovery recovery;
    ABoxEc800Recovery_Init(&recovery, 120000U);
    assert(!ABoxEc800Recovery_Poll(&recovery, 100U, 0, 1));
    assert(!ABoxEc800Recovery_Poll(&recovery, 100U, 1, 0));
    assert(ABoxEc800Recovery_Poll(&recovery, 100U, 1, 1));
    assert(!ABoxEc800Recovery_Poll(&recovery, 119999U, 1, 1));
    assert(ABoxEc800Recovery_Poll(&recovery, 120100U, 1, 1));
    assert(!ABoxEc800Recovery_Poll(&recovery, 120101U, 0, 1));
    ABoxEc800Recovery_Init(&recovery, 100U);
    assert(ABoxEc800Recovery_Poll(&recovery, 0xFFFFFFF0U, 1, 1));
    assert(!ABoxEc800Recovery_Poll(&recovery, 0x00000020U, 1, 1));
    assert(ABoxEc800Recovery_Poll(&recovery, 0x00000054U, 1, 1));
    return 0;
}
