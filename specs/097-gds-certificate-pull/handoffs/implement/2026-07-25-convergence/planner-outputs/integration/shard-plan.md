# Integration shard plan

## S01-integration-01 — T038

- Lifecycle stage: worker_execution
- Capability: integration
- Owns: legacy CMake/Kconfig override translation and its configuration regression test
- Depends on: existing Kconfig symbol and existing source gate
- Must not touch: Kconfig, src/CMakeLists.txt, tasks.md, or unrelated profile/configuration behavior
- Validation: configure Full with Pull forced OFF, build, run test_build_config, inspect generated configuration, and prove certificate-manager object symbols are absent
