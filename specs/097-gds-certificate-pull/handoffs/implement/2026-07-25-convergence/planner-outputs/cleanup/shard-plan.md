# Cleanup shard plan

## S02-cleanup-01 — T039

- Lifecycle stage: worker_execution
- Capability: cleanup
- Owns: internal declaration ownership for `mu_certificate_manager_register`
- Depends on: T038 and the existing certificate-manager implementation
- Must not touch: public API, Push registration, Kconfig, CMake source gates, tasks.md
- Validation: focused Full build and certificate-manager test
