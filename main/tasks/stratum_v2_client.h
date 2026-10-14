#ifndef STRATUM_V2_CLIENT_H_
#define STRATUM_V2_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_transport.h"
#include "global_state.h"

// Runs one Stratum V2 session against pool_id until the connection drops or a
// close is requested. Owns the single-pool transport slot for its lifetime,
// under the same rules as the V1 session (see "transport ownership" in
// stratum_task.c). *got_work is set once the pool has sent a job.
esp_err_t stratum_v2_run(GlobalState *GLOBAL_STATE, int pool_id, bool *got_work);

// Noise handshake plus SetupConnection against pool_id, then disconnects.
bool stratum_v2_probe_pool(GlobalState *GLOBAL_STATE, int pool_id);

// Caller must hold GLOBAL_STATE->transport_lock with the transport slot
// non-NULL for the whole call. Returns the stratum_submit_share() codes.
int stratum_v2_submit_share_locked(GlobalState *GLOBAL_STATE, esp_transport_handle_t transport,
                                   const char *job_id, const char *extranonce_2,
                                   uint32_t ntime, uint32_t nonce, uint32_t rolled_version);

#endif // STRATUM_V2_CLIENT_H_
