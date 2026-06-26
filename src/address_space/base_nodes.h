/* src/address_space/base_nodes.h
 * The library's standard Base Information address space (OPC 10000-5 §6.3, §7;
 * standard NodeIds per OPC 10000-6 Annex A). Provides the mandatory Server object
 * hierarchy so a conformant Nano server exists without the integrator hand-building
 * it: Root(84)/Objects(85)/Types(86)/Views(87)/Server(2253) with references,
 * ServerArray(2254), NamespaceArray(2255), ServerStatus(2256) + State(2259),
 * ServerCapabilities(2268) with ServerProfileArray/LocaleIdArray and OperationLimits
 * (11704) MaxNodesPerRead(11705)/MaxNodesPerBrowse(11710).
 *
 * Scope: static structural nodes + static attributes that fit the current variant
 * model (Boolean/Int32/UInt32/Float/String + arrays). ServerStatus.State is the
 * ServerState enum encoded as Int32 (Running = 0). Live DateTime values
 * (ServerStatus.CurrentTime/StartTime) are a follow-up (US2b) that first needs a
 * DateTime variant type and runtime-bound value sources.
 *
 * Read and Browse consult this address space as a fallback AFTER the integrator's
 * own `config.address_space` (so the standard nodes resolve even when the
 * integrator supplies none, and the integrator can still override by NodeId). */
#ifndef MICRO_OPCUA_BASE_NODES_H
#define MICRO_OPCUA_BASE_NODES_H

#include "micro_opcua/address_space.h"
#include "micro_opcua/platform.h"   /* mu_time_adapter_t */

/* The const, static Base Information address space described above. Never NULL. */
const mu_address_space_t *mu_base_address_space(void);

/* US2b — live ServerStatus timestamps. ServerStatus.CurrentTime (i=2258) and
   ServerStatus.StartTime (i=2257) are DateTime values that can't sit in the const
   table (they need the runtime time adapter). They live instead in caller-owned
   storage (the server struct), bound to the time adapter at mu_server_init via
   mu_base_runtime_init. CurrentTime reads time->get_time() live; StartTime returns
   the captured start time. */
typedef struct {
    const mu_time_adapter_t *time;   /* CurrentTime source */
    opcua_datetime_t start_time;     /* StartTime value (captured at init) */
} mu_base_runtime_t;

/* Storage for the runtime-bound dynamic Server status nodes. Place an instance in
   the (caller-owned, mutable) server struct and pass it to mu_base_runtime_init.
   `space` is the address-space view over the two dynamic nodes. */
typedef struct {
    mu_base_runtime_t rt;
    mu_node_t nodes[2];              /* CurrentTime (2258), StartTime (2257) */
    mu_value_source_t values[2];
    mu_address_space_t space;
} mu_base_runtime_nodes_t;

/* Populate `storage` with the CurrentTime/StartTime nodes whose value sources are
   CALLBACKs bound to `&storage->rt` (so CurrentTime reads `time` live). Call once at
   server init; `start_time` is typically time->get_time() at startup. */
void mu_base_runtime_init(mu_base_runtime_nodes_t *storage,
                          const mu_time_adapter_t *time, opcua_datetime_t start_time);

/* Resolve a NodeId against the integrator's address space first (if any), then the
   runtime-bound dynamic space `dynamic` (if any), then the const base set. `dynamic`
   may be NULL. Returns NULL if found in none. Use this in the read/browse paths. */
const mu_node_t *mu_resolve_node(const mu_address_space_t *user,
                                 const mu_address_space_t *dynamic,
                                 const mu_nodeid_t *node_id);

#endif /* MICRO_OPCUA_BASE_NODES_H */
