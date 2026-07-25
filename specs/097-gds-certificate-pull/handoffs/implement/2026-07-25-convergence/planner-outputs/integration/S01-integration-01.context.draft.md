# T038 context digest draft

Task T038 requires the legacy `-DMUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL=OFF`
override to reach Kconfig and remain disabled in generated configuration.

Relevant requirements:

- `spec.md` FR-007: Pull CU dependency and build gate.
- `spec.md` FR-009: complete compile-out when the CU is disabled.
- `tasks.md` T038: generated symbol must remain undefined and both current
  certificate-manager source objects must be excluded.

Repository facts established by the Integration planner:

- Root `CMakeLists.txt` owns `MUC_OPCUA_KCONFIG_FEATURES` and the legacy cache-to-Kconfig fragment translation.
- `MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL` is missing from that translation list.
- `Kconfig` already defines the symbol with the required dependencies.
- `src/CMakeLists.txt` already gates the certificate-manager source directory and public compile definition on the resolved symbol.
- `tests/unit/test_build_config.c` is the existing cross-profile configuration test.

Keep the change atomic. Do not alter Kconfig, source registration, tasks.md, or
unrelated configuration symbols. Do not commit.
