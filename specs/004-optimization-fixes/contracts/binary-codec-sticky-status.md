# Contract — Binary Codec Sticky Status & Overflow-Safe Bounds

**Finding**: T6, T19, T13, T14 · **FR**: FR-003/015 · **Decision**: D8 · **Files**: `include/micro_opcua/encoding.h`, `src/encoding/*.c`

## Intent

Reduce flash by removing ~294 per-call error checks and harden length math, with
**zero change** to encoded/decoded bytes for valid input.

## Changes

- Add a sticky `status` field to `mu_binary_reader_t` and `mu_binary_writer_t`.
- Once `status != Good`, every subsequent primitive is a no-op that returns the sticky
  status (no buffer access).
- `ensure_bytes`/`ensure_space` STILL run on every call; the bound test becomes
  overflow-safe: `count > length - position` (never `position + count > length`).
- Consolidate hand-rolled little-endian pack/unpack (6 files) into one inline helper;
  collapse signed/unsigned writer twins into one width-parameterized writer.

## Contract rules

- **Byte-identical**: for any sequence of valid operations, the produced bytes (write)
  and parsed values + final position (read) are identical to the pre-change codec
  (SC-003). Round-trip and fixture tests MUST confirm.
- **Bounds preserved**: no read/write may exceed the buffer; malformed-length inputs
  return the cited decoding/encoding StatusCode (OPC-10000-6 §5.2). Overflow-crafted
  lengths (e.g. near `SIZE_MAX`) MUST be rejected, not wrap.
- **Error propagation**: a handler that checks `writer->status`/`reader->status` once
  at the end MUST observe the same first-failure StatusCode it would have gotten from
  the per-call checks.
- **No behavioural divergence on partial failure**: a tripped writer MUST NOT emit
  partial/garbage trailing bytes that a consumer could read (position semantics
  defined and tested).

## Acceptance

- All existing encoding/decoding unit + fuzz tests pass unchanged.
- New test: an early-tripped reader/writer yields the same StatusCode and leaves no
  out-of-bounds access (ASan/UBSan clean).
- Overflow-length fuzz vectors return decoding errors (no wrap).
