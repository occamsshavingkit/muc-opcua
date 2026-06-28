# Research & Design: UserName/Password User Identity Token Authentication

## Conformance Unit Requirements

The target profile is **User Token-UserName Server Facet** (`http://opcfoundation.org/UA-Profile/Server/UserToken-UserName`).
This requires the server to:
1. Advertise `UserNameIdentityToken` in its `UserTokenPolicies` of its `EndpointDescription`.
2. Decode `UserNameIdentityToken` in `ActivateSessionRequest`.
3. Support authenticating the client based on the provided username/password.
4. Support plain-text password validation (over security policy `None`).
5. Support encrypted password validation (over standard secure channels using the server's private key).

## Binary Structure

### UserTokenPolicy (GetEndpoints Response)

We need to encode a `UserTokenPolicy` structure into the endpoint description (defined in OPC-10000-4 §7.37):
```
UserTokenPolicy:
- policyId: String
- tokenType: UserTokenType (enum: 0 = Anonymous, 1 = UserName, 2 = Certificate, 3 = IssuedToken)
- issuedTokenType: String (null)
- issuerEndpointUrl: String (null)
- securityPolicyUri: String (null or security policy URI if specific policy required)
```

In `src/services/discovery.c`, we currently hardcode a single `Anonymous` policy. We should allow configuring user token policies via server configuration or support both `Anonymous` and `UserName` automatically when a user authentication callback is registered.

### UserNameIdentityToken (ActivateSession Request)

Binary encoding for `UserNameIdentityToken` body (NodeId 324, OPC-10000-6 §7.38.3):
```
UserNameIdentityToken body:
- policyId: String
- userName: String
- password: ByteString (encrypted with server public key if secure channel, otherwise plain text)
- encryptionAlgorithm: String
```

### Password Decryption

If the password is encrypted, standard clients will encrypt it using the RSA public key of the server certificate.
The `encryptionAlgorithm` string specifies the decryption algorithm:
- Typically `http://www.w3.org/2001/04/xmlenc#rsa-oaep` (or similar for newer policies).
We must decrypt the password using the `rsa_oaep_decrypt` function of the configured `crypto_adapter` before validation.
If the security policy is `None`, the password is sent as plaintext, and `encryptionAlgorithm` is null/empty.

## Verification & Error Handling

- If the token body is invalid or cannot be decoded: return `Bad_DecodingError`.
- If the credentials do not match: return `Bad_IdentityTokenRejected`.
- If the token encoding NodeId is unknown: return `Bad_IdentityTokenInvalid`.
