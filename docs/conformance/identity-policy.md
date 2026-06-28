# Conformance: Identity Token Policy

Per the Nano Embedded Device Server Profile, the server must support `AnonymousIdentityToken`.
Support for `UserNameIdentityToken` and `X509IdentityToken` is included in the `MICRO_OPCUA_USER_AUTH` option, which is enabled by default in the Embedded and Full-Featured profiles.

- Supported Token Types: Anonymous, UserName, X509
- SecurityPolicy: None, Basic256Sha256
