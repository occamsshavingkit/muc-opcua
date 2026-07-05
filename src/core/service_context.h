/* src/core/service_context.h
 *
 * Lightweight context header for service-layer modules (browse, discovery,
 * session, subscription). Provides the types services need without pulling
 * all core internals through server_internal.h.
 *
 * This is an incremental step toward clean core↔services separation.
 * Service files that currently include server_internal.h can switch to
 * this header for the public-facing types, adding server_internal.h only
 * when they need core-private definitions. */

#ifndef MUC_OPCUA_SERVICE_CONTEXT_H
#define MUC_OPCUA_SERVICE_CONTEXT_H

#include "../address_space/base_nodes.h"
#include "../services/secure_channel.h"
#include "../services/session.h"
#include "../services/subscription.h"
#include "muc_opcua/encoding.h"
#include "muc_opcua/server.h"

#endif /* MUC_OPCUA_SERVICE_CONTEXT_H */
