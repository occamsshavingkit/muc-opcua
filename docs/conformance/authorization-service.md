# Conformance: Authorization Service Configuration Server (spec 093)

This server implements the OPC UA **Authorization Service Configuration
Server** (OPC-10000-7 PG18, CU 3182) — type-system address-space exposure of
the `AuthorizationServiceConfigurationType` and its InstanceDeclarations per
OPC-10000-12 §9.7.4 Table 158. Gated behind
**`MUC_OPCUA_CU_AUTHORIZATION_SERVICE_CONFIGURATION_SERVER`** (default **ON**
for the Full profile; depends on `MUC_OPCUA_CU_USER_TOKEN_JWT` and
`MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`).

This CU is paired with the [User Token — JWT Server Facet](jwt-user-token.md)
(CU 1697), which performs the actual JWT signature and claim validation at
ActivateSession. CU 3182 owns only the address-space **type-system
InstanceDeclarations** that let a client browse the server's trusted
AuthorizationService configuration.

Grounded against:

- OPC-10000-4 §5.7.3 (ActivateSession UserIdentityToken dispatch)
- OPC-10000-7 v1.05.02 CU 3182 (Authorization Service Configuration Server)
- OPC-10000-7 v1.05.02 CU 1697 (User Token JWT Server Facet)
- OPC-10000-12 §9.7.4 (`AuthorizationServiceConfigurationType` Table 158)
- OPC-10000-12 §7.10.14 (`ApplicationConfigurationType.AuthorizationServices`)

## Scope

| NodeId | BrowseName | NodeClass | TypeDefinition | Modelling Rule | DataType |
|---:|---|---|---|---|---|
| 17852 | AuthorizationServiceConfigurationType | ObjectType | BaseObjectType (58) | — | — |
| 18072 | ServiceUri | Variable | PropertyType (68) | Mandatory | String (12) |
| 17860 | ServiceCertificate | Variable | PropertyType (68) | Mandatory | ByteString (15) |
| 18073 | IssuerEndpointUrl | Variable | PropertyType (68) | Mandatory | String (12) |

NodeIds sourced from the OPC Foundation's official UA-Nodeset repository
([OPCFoundation/UA-Nodeset@`6338cced8e6cc2fa2c3816bc6b3bad5daee3f101`](https://github.com/OPCFoundation/UA-Nodeset/blob/6338cced8e6cc2fa2c3816bc6b3bad5daee3f101/Schema/Opc.Ua.NodeSet2.Services.xml#L36884-L36918))
and the companion CSV index
([`Schema/NodeIds.csv` lines 6025-6033](https://github.com/OPCFoundation/UA-Nodeset/blob/6338cced8e6cc2fa2c3816bc6b3bad5daee3f101/Schema/NodeIds.csv#L6025-L6033)
and [lines 6245-6246](https://github.com/OPCFoundation/UA-Nodeset/blob/6338cced8e6cc2fa2c3816bc6b3bad5daee3f101/Schema/NodeIds.csv#L6245-L6246)).
The NodeIds 17853 through 17855 are `WriterGroupType` InstanceDeclaration
nodes defined in OPC-10000-14 §9.1.6.3 and therefore cannot be reused for
`AuthorizationServiceConfigurationType` properties.

## Out of Scope

Per spec 093 Scope Boundaries:

- Full GDS Authorization Service (token issuance, introspection endpoint, OAuth2
  Client Credentials flow server-side). The server is an OAuth2 **Resource
  Server** only, not an Authorization Server.
- Online token introspection (RFC 7662).
- Dynamic JWKS fetching from issuer URL — the integrator provides the public
  key via `mu_jwt_issuer_t.public_key` in the server config.
- Encrypted JWTs (JWE).
- The runtime `AuthorizationServiceConfiguration` Object instances — only the
  type-system InstanceDeclarations are exposed in this revision. A future spec
  may add the `<AuthorizationServiceName>` placeholder Object and its folder.

## Backing Tests

| Claim | Test |
|---|---|
| AuthorizationServiceConfigurationType is browsable as subtype of BaseObjectType | `test_type_system` |
| ActivateSession with a JWT triggers Resource-Server validation | `test_jwt_activate_session` |
| JWT validation rejects unknown issuers and audiences | `test_jwt_multi_issuer` |
| JWT validation honours per-issuer clock skew | `test_jwt_clock_skew` |

## Build Gating

```kconfig
config MUC_OPCUA_CU_AUTHORIZATION_SERVICE_CONFIGURATION_SERVER
    bool "Authorization Service Configuration Server"
    depends on MUC_OPCUA_CU_USER_TOKEN_JWT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    default y if MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
```

When undefined, JWT validation still works (CU 1697 is independent), but no
`AuthorizationServiceConfigurationType` nodes appear in the address space.
