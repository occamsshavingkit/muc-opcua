# Changelog

All notable changes to muc-opcua will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Standard 2017 UA Server Profile (spec 037)
- Comprehensive backlog fixes (spec 038): CI/CD pipeline integrity, UB fixes, code quality decomposition, session timeout gating

### Fixed
- CI pipeline no longer silently ignores static analysis, sanitizer, and fuzz build failures
- Integer underflow guard in `mu_checked_memcpy_off` (safe_mem.h)
- Null-pointer dereference guard in `mu_binary_read_expanded_nodeid` (binary_nodeid.c)
- Session timeout now gated on `MUC_OPCUA_SESSION_TIMEOUT` instead of `MUC_OPCUA_MULTI_CHUNK`
- `mu_session_create` marked as deprecated in favor of `mu_session_create_with_identifiers`

### Changed
- `handle_activate_session` decomposed into per-token-type helpers
- `mu_server_poll` common logic extracted from duplicated `#ifdef` branches
- `handle_create_monitored_items` decomposed into filter-resolution, item-allocation, and response-encode helpers

## [0.1.0] - 2026-07-05

### Added
- Initial version tracking (CHANGELOG.md, version.h)
