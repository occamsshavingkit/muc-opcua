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

/* The const, static Base Information address space described above. Never NULL. */
const mu_address_space_t *mu_base_address_space(void);

/* Resolve a NodeId against the integrator's address space first (if any), then the
   base address space. Returns NULL if found in neither. Use this in the read/browse
   paths instead of calling mu_address_space_find_node directly on one space. */
const mu_node_t *mu_resolve_node(const mu_address_space_t *user, const mu_nodeid_t *node_id);

#endif /* MICRO_OPCUA_BASE_NODES_H */
