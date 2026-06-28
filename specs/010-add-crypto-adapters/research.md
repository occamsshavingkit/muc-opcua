# Research: Cryptographic Platform Adapters

## Decision
Implement native, freestanding cryptographic adapter wrappers for the `Mbed TLS` and `wolfSSL` libraries implementing the `mu_crypto_adapter_t` abstraction layer.

## Rationale
- **Footprint**: OpenSSL is too large for ARM Cortex-M microcontrollers. Mbed TLS and wolfSSL are highly optimized for embedded systems.
- **Pluggability**: The `mu_crypto_adapter_t` interface is fully library-agnostic, allowing the core micro-opcua code to remain identical while changing only the cryptographic engine.
- **Security**: Wraps standard, verified external libraries rather than rolling custom crypto math.

## Alternatives Considered
- **Direct Library Integration**: Hardcoding Mbed TLS or wolfSSL calls directly in the security policy code. (Rejected: ruins the library-agnostic design and makes switching backends difficult).
- **Custom Crypto Primitives**: Implementing custom AES/RSA/SHA math in plain C. (Rejected: high security risk, potential side-channel vulnerabilities).
