# Contract: FreeRTOS + lwIP Getting-Started Guidance

## Consumer

An embedded integrator using FreeRTOS + lwIP as the platform envelope for `micro-opcua`.

## Required Documentation Outcomes

- The guide identifies `MICRO_OPCUA_PLATFORM=external` as the integration mode when the application owns the TCP/IP stack.
- The guide states that `platform/pico` is a skeleton/lifecycle adapter, not a complete TCP/IP integration.
- The guide lists the required `mu_tcp_adapter_t`, `mu_time_adapter_t`, and `mu_entropy_adapter_t` responsibilities.
- The TCP adapter description uses nonblocking lwIP socket behavior: idle accept/read/write returns `MU_STATUS_GOOD` with no connection or zero bytes; EOF and real socket failures close the connection with a bad TCP status.
- The guide shows static server storage and static transport buffers sized with `MU_SERVER_STORAGE_BYTES` and `MU_MIN_CHUNK_SIZE`.
- The guide shows a FreeRTOS task loop that calls `mu_server_poll()` and yields.
- The guide separates library memory from FreeRTOS heap, lwIP heap, task stack, and platform buffers.

## Verification

- `docs/getting-started.md` contains all required outcomes.
- Existing documentation/conformance tests pass.
