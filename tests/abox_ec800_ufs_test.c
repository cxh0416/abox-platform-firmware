#include "abox_ec800_ufs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
typedef struct { uint64_t id; uint32_t length; int drain; ABoxAsyncStatus result; } MockUfs;
static int start(void *context, const ABoxUfsRequest *r, uint64_t *id)
{ MockUfs *m = context; CHECK(r->path); *id = ++m->id; return 1; }
static ABoxAsyncStatus poll(void *context, uint64_t id, uint32_t *length)
{ MockUfs *m = context; if (id != m->id) return ABOX_ASYNC_PENDING; *length = m->length; return m->result; }
static int cancel(void *context, uint64_t id)
{ MockUfs *m = context; CHECK(id == m->id); return m->drain; }
int main(void)
{
    MockUfs mock = {0, 4, 1, ABOX_ASYNC_OK};
    ABoxEc800UfsPort p = {&mock, start, poll, cancel};
    ABoxEc800Ufs u;
    uint8_t data[] = {0, '\r', '\n', 255};
    char path[] = "UFS:ca_v1.pem";
    ABoxUfsRequest r = {ABOX_UFS_UPLOAD, path, data, sizeof(data)};
    CHECK(ABoxEc800Ufs_Init(&u, &p));
    CHECK(!ABoxEc800Ufs_PathValid("UFS:bad\"\r\n"));
    CHECK(!ABoxEc800Ufs_PathValid("UFS:"));
    CHECK(!ABoxEc800Ufs_PathValid("UFS:../ca"));
    CHECK(ABoxEc800Ufs_Start(&u, &r, 0, 10));
    path[4] = 'x'; CHECK(!strcmp(u.request.path, "UFS:ca_v1.pem"));
    CHECK(!ABoxEc800Ufs_Start(&u, &r, 0, 10));
    ABoxEc800Ufs_Poll(&u, 1); CHECK(u.status == ABOX_ASYNC_OK);
    r.operation = ABOX_UFS_READ;
    CHECK(ABoxEc800Ufs_Start(&u, &r, 0, 10));
    mock.length = 3; ABoxEc800Ufs_Poll(&u, 1); CHECK(u.status == ABOX_ASYNC_ERROR);
    CHECK(ABoxEc800Ufs_Start(&u, &r, UINT32_MAX - 5, 10));
    mock.result = ABOX_ASYNC_PENDING; mock.drain = 0;
    ABoxEc800Ufs_Poll(&u, 4); CHECK(u.status == ABOX_ASYNC_QUARANTINED);
    mock.result = ABOX_ASYNC_OK; ABoxEc800Ufs_Poll(&u, 5);
    CHECK(u.status == ABOX_ASYNC_QUARANTINED);
    CHECK(!ABoxEc800Ufs_Start(&u, &r, 0, 10));
    ABoxEc800Ufs_OnModemReset(&u); mock.drain = 1;
    CHECK(ABoxEc800Ufs_Start(&u, &r, 0, 10));
    ABoxEc800Ufs_Cancel(&u); CHECK(u.status == ABOX_ASYNC_CANCELLED);
    r.operation = ABOX_UFS_REMOVE; r.data = NULL; r.size = 0;
    CHECK(ABoxEc800Ufs_Start(&u, &r, 0, 10));
    ABoxEc800Ufs_Poll(&u, 1); CHECK(u.status == ABOX_ASYNC_OK);
    puts("UFS: path validation, request ownership, exact length, cancellation, wraparound timeout and quarantine passed");
    return 0;
}
