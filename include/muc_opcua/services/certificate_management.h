/* include/muc_opcua/services/certificate_management.h
 *
 * Global Certificate Management Server Facet (spec 096, PG20, CU 2105).
 * Type-system InstanceDeclarations for CertificateGroupType and
 * CertificateType. Actual certificate enrollment/renewal Methods
 * (CreateSigningRequest, GetRejectedList, FinishRequest) are deferred.
 * OPC-10000-12 §7.5-7.6, OPC-10000-7 v1.05.02.
 *
 * This is a minimal type-system-only slice: the ObjectType nodes are
 * exposed in the AddressSpace for browsing. When Methods are later
 * implemented, the adapter will gain the actual certificate lifecycle
 * callbacks. A NULL adapter on a server configured with the CU enabled
 * is valid -- the type nodes still appear in the address space.
 */
#ifndef MUC_OPCUA_SERVICES_CERTIFICATE_MANAGEMENT_H
#define MUC_OPCUA_SERVICES_CERTIFICATE_MANAGEMENT_H

#include "muc_opcua/config.h"
#include "muc_opcua/status.h"
#include "muc_opcua/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ObjectType NodeIds in namespace 0 (OPC-10000-12 §7.5-7.6, NodeIds.csv). */
#define MU_ID_CERTIFICATE_GROUP_TYPE 12555u
#define MU_ID_CERTIFICATE_TYPE 12556u

#ifdef MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT

struct mu_server;

/* Integrator-provided certificate management adapter. Placeholder for future
 * Method implementations (CreateSigningRequest, GetRejectedList,
 * FinishRequest). The type nodes exist in the address space regardless of
 * whether an adapter is configured. */
typedef struct {
    void *context;
} mu_certificate_management_adapter_t;

#endif /* MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT */

#ifdef __cplusplus
}
#endif

#endif /* MUC_OPCUA_SERVICES_CERTIFICATE_MANAGEMENT_H */
