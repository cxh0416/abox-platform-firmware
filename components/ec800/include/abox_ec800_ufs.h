#ifndef ABOX_EC800_UFS_H
#define ABOX_EC800_UFS_H
#include "abox_ec800_async.h"
#ifdef __cplusplus
extern "C" {
#endif
#define ABOX_EC800_UFS_PATH_SIZE 64U
typedef enum { ABOX_UFS_UPLOAD, ABOX_UFS_READ, ABOX_UFS_REMOVE } ABoxUfsOperation;
typedef struct {
    ABoxUfsOperation operation;
    const char *path;
    /* Buffer remains alive until completion or successful cancellation.
     * UPLOAD is read-only; READ writes at most size bytes. */
    uint8_t *data;
    uint32_t size;
} ABoxUfsRequest;
typedef struct {
    void *context;
    /* Backend owns QF* framing, CONNECT/raw mode, handles and close recovery.
     * OK means exact transfer and handle closed, not merely command ACK. */
    int (*start)(void *context, const ABoxUfsRequest *request, uint64_t *operation);
    ABoxAsyncStatus (*poll)(void *context, uint64_t operation, uint32_t *transferred);
    int (*cancel)(void *context, uint64_t operation);
} ABoxEc800UfsPort;
typedef struct {
    ABoxEc800UfsPort port;
    ABoxUfsRequest request;
    char path[ABOX_EC800_UFS_PATH_SIZE];
    uint64_t operation;
    uint32_t started, timeout, transferred;
    ABoxAsyncStatus status;
} ABoxEc800Ufs;
int ABoxEc800Ufs_PathValid(const char *path);
int ABoxEc800Ufs_Init(ABoxEc800Ufs *ufs, const ABoxEc800UfsPort *port);
int ABoxEc800Ufs_Start(ABoxEc800Ufs *ufs, const ABoxUfsRequest *request,
                      uint32_t now, uint32_t timeout_ms);
void ABoxEc800Ufs_Poll(ABoxEc800Ufs *ufs, uint32_t now);
void ABoxEc800Ufs_Cancel(ABoxEc800Ufs *ufs);
/* Only after physical modem reset and adapter drain; invalidates old requests. */
void ABoxEc800Ufs_OnModemReset(ABoxEc800Ufs *ufs);
#ifdef __cplusplus
}
#endif
#endif
