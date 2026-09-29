#include "abox_config_store.h"
#include <assert.h>

typedef struct {
    const void *candidate, *stable;
    unsigned calls;
    int candidate_ok, stable_ok;
} Fixture;

static int write_and_readback(void *context, const void *snapshot)
{
    Fixture *fixture = (Fixture *)context;
    ++fixture->calls;
    if (snapshot == fixture->candidate) return fixture->candidate_ok;
    assert(snapshot == fixture->stable);
    return fixture->stable_ok;
}

int main(void)
{
    int candidate = 1, stable = 2;
    Fixture fixture = {&candidate, &stable, 0U, 1, 1};
    ABoxConfigStorePort port = {&fixture, write_and_readback};
    assert(ABoxConfigStore_Commit(&port, &candidate, &stable) ==
           ABOX_CONFIG_STORE_SAVED && fixture.calls == 1U);
    fixture.calls = 0U; fixture.candidate_ok = 0;
    assert(ABoxConfigStore_Commit(&port, &candidate, &stable) ==
           ABOX_CONFIG_STORE_RECOVERED && fixture.calls == 2U);
    fixture.calls = 0U; fixture.stable_ok = 0;
    assert(ABoxConfigStore_Commit(&port, &candidate, &stable) ==
           ABOX_CONFIG_STORE_RECOVERY_FAILED && fixture.calls == 2U);
    fixture.calls = 0U;
    assert(ABoxConfigStore_Commit(&port, &candidate, &candidate) ==
           ABOX_CONFIG_STORE_RECOVERY_FAILED && fixture.calls == 0U);
    return 0;
}
