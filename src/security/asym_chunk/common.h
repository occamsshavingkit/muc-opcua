/* src/security/asym_chunk/common.h
 * Shared declarations for asymmetric chunk wrap/unwrap. */
#ifndef MUC_OPCUA_ASYM_CHUNK_COMMON_H
#define MUC_OPCUA_ASYM_CHUNK_COMMON_H

#include "../../core/safe_mem.h"
#include "../asym_chunk.h"
#include "../certificate.h"
#include "../key_derivation.h"
#include "muc_opcua/encoding.h"
#include <string.h>

/* RSA-OAEP with SHA-1 (MGF1-SHA1) overhead: plaintext block = keybytes - 2*20 - 2. */
#define MU_OAEP_SHA1_OVERHEAD 42

/* RSA modulus-sized scratch for Basic256Sha256. A 2048-bit RSA modulus is
   256 bytes; keeping headroom for 4096-bit RSA requires 512 bytes. RSA-OAEP
   plaintext blocks are smaller than the modulus by MU_OAEP_SHA1_OVERHEAD. */
#define MU_ASYM_SCRATCH 512

/* Signature input is [cleartext header | signed plaintext], not an RSA block.
   Keep the existing envelope limit separate so modulus scratch remains small
   without changing accepted OPN header/body sizes. */
#define MU_ASYM_SIGNED_INPUT_MAX 4096

static opcua_statuscode_t key_bytes(const mu_crypto_adapter_t *crypto, const opcua_byte_t *cert, size_t cert_len,
                                    size_t *bytes) {
    size_t bits = 0;
    opcua_statuscode_t s = crypto->get_certificate_key_bits(crypto->context, cert, cert_len, &bits);
    if (s != MU_STATUS_GOOD || bits == 0) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    *bytes = bits / 8;
    /* The RSA modulus must exceed the OAEP overhead, or the plaintext block size
       would underflow. (Basic256Sha256 also mandates >= 2048-bit keys.) */
    if (*bytes <= MU_OAEP_SHA1_OVERHEAD) {
        return MU_STATUS_BAD_CERTIFICATEINVALID;
    }
    return MU_STATUS_GOOD;
}

#endif /* MUC_OPCUA_ASYM_CHUNK_COMMON_H */
