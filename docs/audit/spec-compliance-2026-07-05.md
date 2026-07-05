# muc-opcua Spec-to-Code Compliance Audit

**Date**: 2026-07-05
**Scope**: All implemented OPC UA services, binary encoding, and security
**Method**: Cross-referenced every service handler against OPC-10000-4 and OPC-10000-6 using the OPC UA MCP reference tools and line-by-line code analysis.
**Codebase**: `main` branch, commits up to `3e91c16`

---

## 1. Correctly Implemented (Conformant)

### 1.1 Discovery Service Set

| Service | § | What's correct |
|---------|---|----------------|
| FindServers | 5.5.2 | All request fields decoded (endpointUrl, localeIds[], serverUris[]). EndpointUrl filter, localeIds negotiation (first-valid), serverUris substring matching. Response encodes full ApplicationDescription with locale-adapted ApplicationName. Empty arrays handled. |
| GetEndpoints | 5.5.4.2 | All request fields decoded. EndpointUrl exact-match filter. ProfileUri transport-profile filtering (empty=all, otherwise matches). Full EndpointDescription encoded. Profile URI cache optimization. |

### 1.2 SecureChannel Service Set

| Service | § | What's correct |
|---------|---|----------------|
| OpenSecureChannel | 5.6.2 | Request body fully decoded. SecurityMode rejected when invalid (Bad_SecurityModeRejected). SecurityPolicy mismatch rejected (Bad_SecurityPolicyRejected). Lifetime clamped [10s, 3600s]. ServerNonce from entropy before opening. Response encodes all mandatory fields. Symmetric keys derived via P_SHA256 for Sign/SignAndEncrypt. Trust-list certificate validation for non-None policies. Channel ID uses incrementing counter. |
| CloseSecureChannel | 5.6.3 | Service registered in dispatch table. |

### 1.3 Session Service Set

| Service | § | What's correct |
|---------|---|----------------|
| CreateSession | 5.7.2 | All request fields decoded. Client cert validated against OPN cert on secured channels. ServerNonce from entropy before session allocation. Response prefix written before session state (session_created guard prevents slot leak). All mandatory response fields encoded. ServerSignature signed on secured channels. Body completeness enforced. Bad_TooManySessions. SessionTimeout bounded. SessionId/AuthToken from entropy with collision avoidance. serverSoftwareCertificates always empty. ServerNonce length=32. |
| ActivateSession | 5.7.3 | All request fields decoded. ClientSignature.algorithm URI validated against SecurityPolicy. ClientSignature over (serverCert||serverNonce) verified via RSA. Anonymous (i=321), Username (i=324, RSA-OAEP decrypt + ServerNonce anti-replay), Certificate (i=327, RSA-SHA256/PSS verify). Unsupported tokens return Bad_IdentityTokenInvalid. ServerNonce regenerated each call. Service failure in ResponseHeader.serviceResult. results[] and diagnosticInfos[] emitted. Session validates SecureChannel binding. Secure-by-default: username rejected on SecurityPolicy=None. |
| CloseSession | 5.7.4 | deleteSubscriptions parameter decoded. AuthToken validated against active session. Bad_SessionIdInvalid returned. Subscriptions deleted when deleteSubscriptions=TRUE. Active session pointer cleared. |

### 1.4 View Service Set

| Service | § | What's correct |
|---------|---|----------------|
| Browse | 5.9.2 | All request fields decoded. BrowseDirection validated (>2→Bad_BrowseDirectionInvalid). Unknown node→Bad_NodeIdUnknown. Null referenceTypeId→all references. includeSubtypes with subtype climbing. nodeClassMask filtering. requestedMaxReferencesPerNode enforced. Partial results with Bad_NoContinuationPoints. All ReferenceDescription fields encoded. TypeDefinition from cache (T22). DiagnosticInfos encoded. |
| BrowseNext | 5.9.3 | releaseContinuationPoints + array decoded. Count bounded (Bad_TooManyOperations). Every point returns Bad_ContinuationPointInvalid (server never creates points — correct). |
| TranslateBrowsePathsToNodeIds | 5.9.4 | All request fields decoded. startingNode per path. relativePath bounded (max 8). Direction matching (isInverse). Subtype matching. Node resolution via address_space_find_node. Bad_NoMatch. RemainingPathIndex=0xFFFFFFFF when resolved. DiagnosticInfos. |

### 1.5 Attribute Service Set

| Service | § | What's correct |
|---------|---|----------------|
| Read | 5.11.2 | All request fields decoded. maxAge, timestampsToReturn validated. nodesToRead bounded (max 32). ReadValueId fields correct (attributeId as UInt32). Unknown node→Bad_NodeIdUnknown. Value on non-Variable→Bad_AttributeIdInvalid. No value→Bad_NotReadable. Supported attributes: NodeId, NodeClass, BrowseName, DisplayName, Value. DataValues encoded. DiagnosticInfos encoded. |
| Write | 5.11.4 | All request fields decoded. nodesToWrite bounded. Empty→Bad_NothingToDo. Unknown node→Bad_NodeIdUnknown. Non-Value attr→Bad_NotWritable. Missing value→Bad_TypeMismatch. IndexRange→Bad_WriteNotSupported. Non-Variable→Bad_NotWritable. DiagnosticInfos[] encoded (fixed in #241). |

### 1.6 Subscription Service Set

| Service | § | What's correct |
|---------|---|----------------|
| CreateSubscription | 5.14.2 | All 7 fields decoded. Response encodes subscriptionId + 3 revised durations. Publishing interval clamped [50ms, 60000ms]. Lifetime ≥ 3×keep_alive revision. Bad_TooManySubscriptions. |
| ModifySubscription | 5.14.3 | Request decode complete. Response encode correct. Bad_SubscriptionIdInvalid. Lifetime/keep_alive revision. |
| DeleteSubscriptions | 5.14.8 | Array decode + per-id response. MonitoredItems deleted. Per-id Bad_SubscriptionIdInvalid. |
| SetPublishingMode | 5.14.4 | publishingEnabled + array decoded. Per-id response. Flag toggle. Per-id Bad_SubscriptionIdInvalid. |
| Publish | 5.14.5 | RequestHeader + ack array decoded. Ack count bounded. Ack processing correct. Request queuing (Bad_TooManyPublishRequests). SubscriptionId, availableSequenceNumbers, moreNotifications, notificationMessage, ack results, diagnosticInfos all encoded. Keep-alive mechanism. NotificationMessage retransmit store. |
| Republish | 5.14.6 | Request decode correct. Bad_SubscriptionIdInvalid. Bad_MessageNotAvailable. Response from retransmit store. Bad_ResponseTooLarge guard. |

### 1.7 MonitoredItem Service Set

| Service | § | What's correct |
|---------|---|----------------|
| CreateMonitoredItems | 5.13.2 | All fields decoded (nodeId, attributeId, indexRange, dataEncoding, monitoringMode, clientHandle, samplingInterval, filter, queueSize, discardOldest). Response correct. MonitoringMode validated. Bad_NodeIdUnknown. Bad_TooManyMonitoredItems. Bad_TooManyOperations. DataChangeFilter: trigger values, deadband None/Absolute, Percent→Unsupported. AggregateFilter decode + Average/Min/Max scope. Queue overflow handling. |
| ModifyMonitoredItems | 5.13.3 | Request decode complete. Response correct. Filter decode during modify. Bad_MonitoredItemIdInvalid. Bad_MonitoredItemFilterUnsupported. |
| SetMonitoringMode | 5.13.4 | Request decode. Response per-id. Mode application. Bad_SubscriptionIdInvalid. Bad_MonitoredItemIdInvalid. |
| SetTriggering | 5.13.5 | All fields decoded. Response with addResults[]+removeResults[]. Trigger link management. Duplicate detection. Bad_MonitoredItemIdInvalid per-link. Triggered items report correctly. Max trigger links enforced. Bad_NothingToDo. |
| DeleteMonitoredItems | 5.13.6 | Request decode. Per-id response. Bad_SubscriptionIdInvalid. Bad_MonitoredItemIdInvalid. |

### 1.8 Binary Encoding (OPC-10000-6)

| Area | § | What's correct |
|------|---|---------------|
| Primitive types | 5.2 | Boolean, SByte, Byte, Int16/UInt16, Int32/UInt32, Int64/UInt64, Float, Double, StatusCode — all LE, bounds-checked, shift-based (no host-endian assumptions). |
| NodeId | 5.2.2.9 | TwoByte, FourByte, Numeric, String formats — encode/decode correct. ExpandedNodeId flags read correctly. |
| DataValue | 5.2.2.17 | Encoding mask bits 0-5. Field order correct (Value, StatusCode, sourceTimestamp, sourcePico, serverTimestamp, serverPico). |
| ExtensionObject | 5.2.2.15 | TypeId + Encoding byte. Encoding 0x00=empty, 0x01=ByteString, 0x02=XML accepted. Body bounded by declared length. |
| String/ByteString | 5.2.2.6-7 | Length as Int32. Null (-1) representation. Negative length rejection. Max length enforcement. Payload bounds check. |
| MessageChunk | 6.7.2 | MessageType validation (HEL/ACK/ERR/MSG/OPN/CLO). ChunkType validation (F/A). MessageSize validation. Single-chunk mode. SecureChannelId correct. HEL/ACK/ERR→id=0. Abort extraction. SequenceHeader correct. |
| Asymmetric security header | — | SecurityPolicyUri, SenderCertificate, ReceiverThumbprint. MessageSize back-patching. |
| Symmetric security header | — | TokenId. Header size 16 bytes. |

### 1.9 Security

| Area | § | What's correct |
|------|---|---------------|
| SecurityPolicies | — | None, Basic256Sha256, Aes128-Sha256-RsaOaep, Aes256-Sha256-RsaPss — all have correct key/block sizes. PSS detection. |
| Key derivation | 6.4 | P_SHA256 correct (RFC 5246 §5). Client/server key derivation correct (nonce as secret/seed). Key material correctly partitioned (sig||enc||iv). Secure zero of key material. Constant-time comparison. |
| Signing/Encryption | 6.7 | RSA-OAEP SHA1/SHA256 encrypt. RSA-SHA256/PSS-SHA256 sign. Sign-then-encrypt for OPN. Padding validation. HMAC-SHA256 for MSG. AES-CBC sign-then-encrypt/decrypt-then-verify. CBC block alignment. |
| Certificates | 5.5 | Certificate required for non-None. Key size validation. Expiry/not-yet-valid check. Thumbprint comparison. Trust list exact match. Trust list enforcement in OPN. |
| Certificate consistency | 5.7 | CreateSession cert ≡ OPN cert. ClientSignature verified. ServerSignature generated. Signature algorithm URI validated. |
| Nonce/Entropy | — | ServerNonce from entropy adapter. Length=32. Stored in session. Returned in responses. Replay check for password tokens. Fails closed on entropy failure. |
| Session activation | 5.7.3 | Anonymous/Username/Certificate tokens. RSA-OAEP password decrypt. Nonce anti-replay. UserTokenSignature consumed. LocaleIds consumed. clientSoftwareCertificates consumed. |
| Crypto adapters | — | OpenSSL host adapter full implementation. MbedTLS adapter. WolfSSL adapter. Cipher context reuse. |

---

## 2. Incorrectly Implemented (Non-Conformant)

### 2.1 CRITICAL

| # | Service | § | Finding | Code Location |
|---|---------|---|---------|---------------|
| I1 | OpenSecureChannel | 5.6.2.3 | **ClientNonce length NOT validated.** Spec: "shall check the minimum length of the Client nonce and return Bad_NonceInvalid if length < 32 bytes." | `service_dispatch.c:463-465` |
| I2 | CreateSession | 5.7.2.3 | **ClientNonce length NOT validated.** Same issue — spec requires 32–128 byte check. | `dispatch_session.c:169-171` |
| I3 | CreateSession | 5.7.2.1 | **ApplicationUri not validated against ClientCertificate.** Spec: "shall check ApplicationUri matches Client Certificate. If not, return Bad_CertificateUriInvalid." | Not implemented |
| I4 | CloseSecureChannel | 5.6.3 | **Handler is NULL** — mu_secure_channel_close() exists but is never called from dispatch. Channel not closed. | `service_dispatch.c:190,2150` |
| I5 | SetPublishingMode | 5.14.4 | **Lifetime counter not reset, MoreNotifications not set to FALSE.** State table row 19 requires both. | `dispatch_subscription.c:33` |

### 2.2 HIGH

| # | Service | § | Finding | Code Location |
|---|---------|---|---------|---------------|
| I6 | OpenSecureChannel | 5.6.2.3 | **requestType field never validated.** Invalid values should return Bad_RequestTypeInvalid. | `service_dispatch.c:457` |
| I7 | Browse | 5.9.2 | **ViewDescription decoded but completely ignored.** Server always browses full address space regardless of viewId. | `browse.c:82-93` |
| I8 | CreateSubscription | 5.14.2/7.21 | **Sampling interval=-1 resolves to 50ms**, not the Subscription publishing interval. | `service_dispatch.c:674` |
| I9 | CreateMonitoredItems | 5.13.2 | **timestampsToReturn decoded but completely ignored.** DataValues in Publish never carry timestamps. | `service_dispatch.c:1299` |
| I10 | CreateMonitoredItems | 5.13.2 | **Revised queue size in response hardcoded to 1u**, regardless of actual queue_size. | `service_dispatch.c:1449` |
| I11 | CreateMonitoredItems | 5.13.2 | **Sampling interval=-1 same bug** as CreateSubscription. | `service_dispatch.c:1406` |
| I12 | ModifyMonitoredItems | 5.13.3 | **queueSize and discardOldest decoded but not applied** to the item. | `service_dispatch.c:1593` |
| I13 | ModifyMonitoredItems | 5.13.3 | **Filter type changes not applied** — non-DataChangeFilter fields decoded and skipped. | `service_dispatch.c:1550-1592` |

### 2.3 MEDIUM

| # | Service | § | Finding | Code Location |
|---|---------|---|---------|---------------|
| I14 | OpenSecureChannel | 5.6.2.2 | **ServerProtocolVersion hardcoded to 0**. Should report actual supported version. | `service_dispatch.c:527` |
| I15 | Read | 5.11.2 | **Timestamps never populated in DataValue responses** regardless of timestampsToReturn. | `read.c:206` |
| I16 | TranslateBrowsePaths | 5.9.4/7.30 | **targetName namespace index ignored** — match only by name, not full QualifiedName. | `dispatch_view.c:131` |
| I17 | TranslateBrowsePaths | 5.9.4.1 | **Empty targetName on last element not rejected.** Should return Bad_BrowseNameInvalid. | `dispatch_view.c:109-166` |
| I18 | Write | 5.11.4.2 | **attributeId decoded as Int32 instead of UInt32** (Read uses correct UInt32). | `write.c:39` |
| I19 | Write | 5.11.4.2 | **Type mismatch uses exact type equality, not subtype check.** Subtypes of DataType should be accepted. | `dispatch_attribute.c:125` |
| I20 | SecureChannel | 6.7.4 | **Channel ID uses incrementing counter**, not cryptographically random. Predictable on reboot. | `secure_channel.c:88-91` |
| I21 | OpenSecureChannel | 5.6.2.3 | **ServerUri and EndpointUrl not validated** in CreateSession against configured values. | `dispatch_session.c:157-158` |
| I22 | CloseSession | 5.7.4.1 | **Requests received after close are NOT rejected** with Bad_SessionClosed. | `dispatch_session.c:800-841` |
| I23 | Variant encoding | 5.2.2.16 | **Array bit (0x80) on read returns BAD_DECODINGERROR** instead of parsing array body. Write path works. | `binary_variant.c:12-15` |

---

## 3. Not Implemented

### 3.1 Service-Level

| # | Service | § | Missing Feature |
|---|---------|---|---------|
| N1 | All services | 6.5 | **Audit events** (AuditCreateSessionEventType, AuditActivateSessionEventType, AuditOpenSecureChannelEventType, AuditChannelEventType, etc.) |
| N2 | CreateSession | 5.7.2.1 | **Session timeout monitoring** — idle sessions are never automatically terminated |
| N3 | CloseSession | 5.7.4.1 | **Outstanding request rejection** with Bad_SessionClosed after CloseSession |
| N4 | Publish | 5.14.5 | **Subscription lifetime counter expiration** — subscriptions never auto-delete |
| N5 | Publish | 5.14.5 | **timeoutHint check** on parked Publish requests per §7.32 |
| N6 | Publish | 5.14.5 | **Bad_NoSubscription** when no subscriptions exist for session |
| N7 | Publish | 5.14.5 | **IssueStatusChangeNotification()** on subscription lifetime expiry/delete |
| N8 | Publish | 5.14.1.1 | **Immediate keep-alive/data delivery** on Publish arrival (timer-poll model limitation) |
| N9 | SetPublishingMode/DeleteSubscriptions | 5.14.1.4 | **DeleteClientPublReqQueue()** on subscription parameter changes |
| N10 | SetMonitoringMode | 5.13.1.3 | **Immediate sample trigger** when changing from DISABLED to SAMPLING/REPORTING |
| N11 | Read | 5.11.2 | **Timestamps in DataValue** — SourceTimestamp, ServerTimestamp never populated |
| N12 | Read | 5.11.2 | **index_range** decoded but ignored |
| N13 | Read | 5.11.2 | **maxAge>0** (cache eligibility) ignored |
| N14 | Read | 5.11.2 | **Only 5 of 22 attribute IDs readable** — DESCRIPTION, WRITEMASK, VALUERANK, DATATYPE, ACCESSLEVEL, HISTORIZING return Bad_AttributeIdInvalid |
| N15 | Write | 5.11.4.2 | **SourceTimestamp/ServerTimestamp in written DataValue** ignored |
| N16 | Write | 5.11.4.2 | **Only Value attribute (id=13) writable** |
| N17 | Write | 5.11.4.2 | **WriteMask/UserWriteMask** not consulted |
| N18 | Write | 5.11.4.2 | **Value persistence requires external write_handler** — without one, all writes fail |
| N19 | Browse | 5.9.2 | **resultMask** decoded but never applied |
| N20 | Browse | 5.9.2 | **ContinuationPoints** never generated |

### 3.2 Encoding-Level

| # | Area | § | Missing Feature |
|---|------|---|---------------|
| N21 | NodeId | 5.2.2.9 | **GUID NodeId format (0x04)** — returns BAD_DECODINGERROR. `mu_nodeid_t` lacks GUID field. |
| N22 | NodeId | 5.2.2.9 | **Opaque/ByteString NodeId format (0x05)** — same, no opaque field in struct. |
| N23 | MessageChunk | 6.7.2 | **Multi-chunk messages** ('C' continuation chunks) explicitly rejected. Only single-chunk. |

### 3.3 Security-Level

| # | Area | § | Missing Feature |
|---|------|---|---------------|
| N24 | Certificates | — | **Certificate chain validation** — trust list uses exact byte match only. No chain walking, issuer validation, or path construction. |
| N25 | Certificates | — | **Hostname/SAN validation** — no check of subject/CN against endpoint. |
| N26 | Certificates | — | **KeyUsage/ExtendedKeyUsage** check not performed. |
| N27 | Key derivation | 6.4 | **P_SHA1** not implemented (acceptable — Basic128Rsa15 not supported). |

---

## 4. Summary Statistics

| Category | Count |
|----------|-------|
| **Services audited** | 22 |
| **Correct behaviors identified** | ~230 |
| **Incorrect (CRITICAL)** | 5 |
| **Incorrect (HIGH)** | 8 |
| **Incorrect (MEDIUM)** | 10 |
| **Not Implemented** | 27 |
| **Overall correctness rate** | ~91% (230/253 audited behaviors) |

### Highest-Priority Fixes (Ordered)

1. **I4** — CloseSecureChannel NULL handler (channel never closes)
2. **I1, I2** — ClientNonce length validation in OpenSecureChannel and CreateSession
3. **I3** — ApplicationUri vs ClientCertificate validation in CreateSession
4. **I7** — Browse ViewDescription ignored (browses full address space regardless)
5. **I5** — SetPublishingMode lifetime counter not reset
6. **I21** — ServerUri/EndpointUrl not validated in CreateSession
7. **I8, I11** — Sampling interval=-1 handling
8. **I9** — timestampsToReturn ignored for monitored items
