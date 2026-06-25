# Traceability: OPC UA Sections to Files

This document maps explicit OPC UA normative sections to implementation and test files.

| OPC UA Part | Section | Description | Files |
|-------------|---------|-------------|-------|
| 3 | 5.2.1 | Base NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/address_space.c` |
| 3 | 5.5.1 | Object NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/address_space.c` |
| 3 | 5.6.2 | Variable NodeClass | `include/micro_opcua/address_space.h`, `src/address_space/value_source.c` |
| 3 | 5.9 | NodeClass Attributes | `include/micro_opcua/address_space.h`, `src/address_space/value_source.c` |
| 4 | 5.5.1 | Discovery Service Set | |
| 4 | 5.5.2 | FindServers | |
| 4 | 5.5.4 | GetEndpoints | |
| 4 | 5.6.2 | OpenSecureChannel | |
| 4 | 5.6.3 | CloseSecureChannel | |
| 4 | 5.7.2 | CreateSession | |
| 4 | 5.7.3 | ActivateSession | |
| 4 | 5.7.4 | CloseSession | |
| 4 | 5.9.2 | Browse | |
| 4 | 5.11.2 | Read | |
| 4 | 7.29 | ReferenceDescription | |
| 4 | 7.32 | RequestHeader | |
| 4 | 7.33 | ResponseHeader | |
| 4 | 7.38.2 | Common StatusCodes | |
| 4 | 7.40.1 | UserIdentityToken | |
| 4 | 7.40.3 | AnonymousIdentityToken | |
| 4 | 7.41 | UserTokenPolicy | |
| 6 | 5.2.1 | OPC UA Binary DataEncoding | |
| 6 | 5.2.2.4 | String Encoding | |
| 6 | 5.2.2.9 | NodeId Encoding | |
| 6 | 5.2.2.15 | ExtensionObject Encoding | |
| 6 | 5.2.2.16 | Variant Encoding | |
| 6 | 5.2.2.17 | DataValue Encoding | |
| 6 | 5.2.5 | Array Encoding | |
| 6 | 5.2.9 | Message Body Encoding | |
| 6 | 6.7.1 | Secure Conversation | |
| 6 | 6.7.2 | MessageChunk Structure | |
| 6 | 6.7.3 | Chunk Type | |
| 6 | 6.7.4 | SecureChannel Establishment | |
| 6 | 6.7.7 | Sequence Numbers | |
| 6 | 7.1.2.3 | Hello Message | |
| 6 | 7.1.2.4 | Acknowledge Message | |
| 6 | 7.1.5 | OPC UA TCP Errors | |
| 6 | 7.2 | OPC UA TCP | |
| 7 | 3.1.5 | Facets | |
| 7 | 4.2 | Conformance Units and Groups | |
| 7 | 4.4 | Profile Categories | |
| 7 | 4.6 | Profile Definitions | |
| 7 | 4.7 | Profile Versioning | |
| 7 | 4.8 | Applications | |
