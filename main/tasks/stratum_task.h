#ifndef STRATUM_TASK_H_
#define STRATUM_TASK_H_

#include <stdbool.h>
#include <stdint.h>
#include <lwip/sockets.h>
#include "esp_err.h"
#include "esp_transport.h"
#include "global_state.h"

typedef struct
{
    double stratum_difficulty;
} SystemTaskModule;

// Returned by stratum_submit_share() when the pool is not currently connected.
#define STRATUM_SUBMIT_NO_CONNECTION (-1000)

#define STRATUM_TRANSPORT_TIMEOUT_MS 5000

typedef struct {
    struct sockaddr_storage dest_addr;
    socklen_t addrlen;
    int addr_family;
    int ip_protocol;
    char host_ip[INET6_ADDRSTRLEN + 16];
} stratum_connection_info_t;

void stratum_task(void *pvParameters);

// Worker-only. Closing a transport from any task other than the one blocked in
// its read is a use-after-free; other tasks must use the request_* variants,
// which only unblock the worker and leave the destroying to it.
void stratum_close_connection(GlobalState * GLOBAL_STATE);
void stratum_close_pool_connection(GlobalState * GLOBAL_STATE, int pool_id);

// Safe from any task.
void stratum_request_close(GlobalState * GLOBAL_STATE);
void stratum_request_pool_close(GlobalState * GLOBAL_STATE, int pool_id);

// version_bits is the Stratum V1 rolled-bits field, rolled_version the full
// version the ASIC hashed (what Stratum V2 submits).
int stratum_submit_share(GlobalState * GLOBAL_STATE, int pool_id, stratum_protocol_t protocol,
                         const char *username, const char *job_id, const char *extranonce_2,
                         uint32_t ntime, uint32_t nonce, uint32_t version_bits, uint32_t rolled_version);

// Single-pool mode: hands a job to create_jobs_task. Takes ownership.
void stratum_enqueue_notify(GlobalState * GLOBAL_STATE, int pool_id, mining_notify *notification,
                            const char *extranonce_str, int extranonce_2_len);

// Checks whether pool_id answers with the protocol it is configured for.
// Opens and closes its own connection; never touches the mining session.
bool stratum_probe_pool(GlobalState * GLOBAL_STATE, int pool_id);

int stratum_get_next_uid(GlobalState * GLOBAL_STATE);
int stratum_get_next_pool_uid(GlobalState * GLOBAL_STATE, int pool_id);

// Shared with the Stratum V2 client, which runs on the same single-pool
// transport slot as the V1 session.
bool is_wifi_connected(void);
void cleanQueue(GlobalState * GLOBAL_STATE);
esp_err_t stratum_resolve_address(const char *hostname, uint16_t port, stratum_connection_info_t *conn_info);
void stratum_set_socket_options(esp_transport_handle_t transport);
const char *stratum_pool_label(int pool_id);

#endif
