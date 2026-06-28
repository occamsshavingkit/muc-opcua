/* src/security/asym_chunk.h
 * Asymmetric (OpenSecureChannel) message-chunk protection for SecurityPolicy
 * Basic256Sha256 (OPC 10000-6 §6.7.2). An OPN chunk is always signed and
 * encrypted when the policy is not None: the AsymmetricAlgorithmSecurityHeader
 * (SecurityPolicyUri, SenderCertificate, ReceiverCertificateThumbprint) is sent
 * in cleartext; the SequenceHeader + body + padding + signature that follow are
 * signed with the sender's private key (RSA-PKCS1.5-SHA256) and encrypted with
 * the receiver's public key (RSA-OAEP-SHA1).
 *
 * mu_asym_chunk_wrap signs with `crypto`'s own key and embeds `crypto`'s own
 * certificate as the SenderCertificate; mu_asym_chunk_unwrap decrypts with
 * `crypto`'s own key and verifies against the SenderCertificate in the chunk. */
#ifndef MICRO_OPCUA_ASYM_CHUNK_H
#define MICRO_OPCUA_ASYM_CHUNK_H

#include "micro_opcua/platform.h"
#include "micro_opcua/status.h"
#include "security_policy.h"
#include <stddef.h>

/* Largest plaintext (SequenceHeader + body + padding + signature) we protect in
   one asymmetric chunk. OPN messages are small; this is generous headroom. */
#define MU_ASYM_MAX_PLAINTEXT 2048

typedef struct {
    mu_security_policy_id_t policy;
    opcua_uint32_t secure_channel_id;
    opcua_uint32_t sequence_number;
    opcua_uint32_t request_id;
    const opcua_byte_t *sender_cert;   /* points into the chunk's cleartext header */
    size_t sender_cert_len;
} mu_asym_chunk_info_t;

/* Build an OPN chunk carrying `body` (an encoded service message, without the
   SequenceHeader, which this function writes). For Basic256Sha256 the result is
   signed+encrypted to `receiver_cert`; for None the body is framed in cleartext
   and receiver_cert is ignored. Returns the full chunk length in *out_len. */
opcua_statuscode_t mu_asym_chunk_wrap(
    const mu_crypto_adapter_t *crypto,
    mu_security_policy_id_t policy,
    opcua_uint32_t secure_channel_id,
    opcua_uint32_t sequence_number,
    opcua_uint32_t request_id,
    const opcua_byte_t *receiver_cert, size_t receiver_cert_len,
    const opcua_byte_t *body, size_t body_len,
    opcua_byte_t *out, size_t out_cap, size_t *out_len);

/* Parse, decrypt, and verify an OPN chunk, recovering the message body (without
   the SequenceHeader). For Basic256Sha256, validates that the receiver thumbprint
   matches our certificate and that the sender signature verifies. Fills `info`
   (policy, ids, and a pointer to the sender certificate within `chunk`). */
opcua_statuscode_t mu_asym_chunk_unwrap(
    const mu_crypto_adapter_t *crypto,
    const opcua_byte_t *chunk, size_t chunk_len,
    opcua_byte_t *out_body, size_t out_cap, size_t *out_body_len,
    opcua_byte_t *scratch, size_t scratch_len,
    mu_asym_chunk_info_t *info);

#endif /* MICRO_OPCUA_ASYM_CHUNK_H */
