# Contract: ActivateSession ClientSignature verification

**Findings**: 2 (signature never verified), 5 (cert not bound), 10 (username
nonce) · **Spec**: FR-001, FR-002, FR-005, FR-006 · **OPC UA**: OPC-10000-4
§5.6.3, §5.7.3, §7.38.2.

## CreateSession (`handle_create_session`)

1. Decode the ClientCertificate as today.
2. **NEW (FR-005)**: for any non-None policy, require
   `client_cert` byte-equals the certificate that opened the current
   SecureChannel. On mismatch → respond `Bad_SecurityChecksFailed`, do not create
   the session.
3. Continue building `ServerSignature` and `ServerNonce` as today. `ServerNonce`
   MUST come from a real entropy source (Finding 1 on RP2040).

## ActivateSession (`handle_activate_session`)

1. Decode `ClientSignature = { algorithm(String), signature(ByteString) }`.
2. **NEW (FR-001/FR-002)**: for any non-None policy, verify:

   ```
   verify_data = serverCert ‖ serverNonce      // current session's nonce
   crypto->rsa_sha256_verify(ctx,
       channelClientCert, channelClientCertLen,  // key source = OPN/CreateSession cert
       verify_data, verify_data_len,
       signature.data, signature.length)
   ```

   - `serverCert` from `crypto->get_own_certificate`.
   - `serverNonce` = `session->server_nonce` (the value returned by *this*
     session's CreateSession — binds liveness, defeats replay).
   - On non-`Good` verify, or `algorithm` not the policy's expected URI, or
     zero-length signature → respond `Bad_ApplicationSignatureInvalid`
     (or `Bad_SecurityChecksFailed`), do not activate.
3. `SecurityPolicy None` → no ClientSignature required; behavior unchanged.

## Username identity token (`handle_activate_session`, i=324 path)

- **NEW (FR-006)**: run the ServerNonce anti-replay comparison for *all* username
  tokens, not only when the password is RSA-encrypted.
- Use the constant-time `mu_secure_memeq` (as used in `sym_chunk`/`asym_chunk`)
  instead of `memcmp`.

## Buffer placement (FR-009 / Finding 4)

- `to_sign`, `sig`, `verify_buf`, `decrypt_buf` MUST live in the `secure_scratch`
  sub-region, not on the handler stack frame. `_Static_assert` proves no overlap
  with the concurrently-live response region.

## Tests (RED first)

| Case | Expected |
|------|----------|
| Correctly-signed ActivateSession, secured policy | activates (Good) |
| Signature over wrong key | `Bad_ApplicationSignatureInvalid` |
| Signature over a prior/stale ServerNonce (replay) | rejected |
| Tampered `verify_data` (cert or nonce byte flipped) | rejected |
| CreateSession cert ≠ OPN cert | `Bad_SecurityChecksFailed` at CreateSession |
| Username token, no token encryption | nonce still checked; replay rejected |
| SecurityPolicy None ActivateSession | unchanged (no signature required) |
| Stack-usage check on the two handlers | no single local > 1 KB |
