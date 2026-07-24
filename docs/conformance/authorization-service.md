# Conformance: Authorization Service Server Facet (spec 093)

This server implements the OPC UA **Authorization Service Server Facet**
(OPC-10000-7 PG18, CU 1629) — address-space exposure of the
`AuthorizationServiceConfigurationType` and its InstanceDeclarations per
OPC-10000-12 §9.7.4 Table 158. Gated behind
**`MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER`** (default **ON** for the Full
profile; depends on `MUC_OPCUA_CU_USER_TOKEN_JWT` and
`MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`).

This facet is paired with the [User Token — JWT Server Facet](jwt-user-token.md)
(CU 1697), which performs the actual JWT signature and claim validation at
ActivateSession. CU 1629 only owns the address-space **type-system
InstanceDeclarations** that let a client browse the server's trusted
AuthorizationService configuration.

Grounded against:

- OPC-10000-4 §5.7.3 (ActivateSession UserIdentityToken dispatch)
- OPC-10000-7 v1.05.02 CU 1629 (Authorization Service Server Facet)
- OPC-10000-12 §9.7.4 (`AuthorizationServiceConfigurationType` Table 158)
- OPC-10000-12 §7.10.14 (`ApplicationConfigurationType.AuthorizationServices`)

## Scope

| NodeId | BrowseName | NodeClass | TypeDefinition | Modelling Rule | DataType |
|---:|---|---|---|---|---|
| 17852 | AuthorizationServiceConfigurationType | ObjectType | BaseObjectType (58) | — | — |
| 17853 | ServiceUri | Variable | PropertyType (68) | Mandatory | String (12) |
| 17854 | ServiceCertificate | Variable | PropertyType (68) | Mandatory | ByteString (15) |
| 17855 | IssuerEndpointUrl | Variable | PropertyType (68) | Mandatory | String (12) |

The property NodeIds (17853/17854/17855) are project-local allocations; the
OPC UA spec ships BrowseNames only for these InstanceDeclarations. They live
in the 17xxx range alongside the type NodeId.

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
config MUC_OPCUA_CU_AUTHORIZATION_SERVICE_SERVER
    bool "Authorization Service Server"
    depends on MUC_OPCUA_CU_USER_TOKEN_JWT && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION
    default y if MUC_OPCUA_INTERN_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES
```

When undefined, JWT validation still works (CU 1697 is independent), but no
`AuthorizationServiceConfigurationType` nodes appear in the address space.
