#ifndef ABOX_CONFIG_STORE_H
#define ABOX_CONFIG_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* The product owns its Flash ABI and provides two immutable snapshots. The
 * callback must write the selected snapshot and verify it by reading Flash
 * back before returning success. No Platform code interprets its bytes. */
typedef struct {
    void *context;
    int (*write_and_readback)(void *context, const void *snapshot);
} ABoxConfigStorePort;

typedef enum {
    ABOX_CONFIG_STORE_SAVED = 0,
    ABOX_CONFIG_STORE_RECOVERED,
    ABOX_CONFIG_STORE_RECOVERY_FAILED
} ABoxConfigStoreResult;

/* A candidate failure always attempts to restore and verify the old snapshot.
 * RECOVERED means the candidate was rejected; only SAVED may advance a trial.
 * RECOVERY_FAILED requires the caller to block further network changes. */
ABoxConfigStoreResult ABoxConfigStore_Commit(const ABoxConfigStorePort *port,
                                             const void *candidate,
                                             const void *stable);

#ifdef __cplusplus
}
#endif
#endif
