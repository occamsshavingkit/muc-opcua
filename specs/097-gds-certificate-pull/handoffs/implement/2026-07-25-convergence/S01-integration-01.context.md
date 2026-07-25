# T038 context digest

Implement only T038.

Root `CMakeLists.txt` translates listed `MUC_OPCUA_KCONFIG_FEATURES` cache
overrides into a Kconfig fragment. The Pull CU symbol exists in Kconfig and its
source directory is already gated in `src/CMakeLists.txt`, but the symbol is
absent from the legacy translation list. Add it there.

Extend `tests/unit/test_build_config.c`, the existing cross-profile build
configuration test, to cover the Pull-CU definition state. The Full profile
configured with `-DMUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL=OFF` must resolve the
symbol OFF, omit its generated preprocessor definition, and omit the current
certificate-manager object symbols.

Grounding: `spec.md` FR-007 and FR-009; `tasks.md` T038. Do not change Kconfig,
`src/CMakeLists.txt`, tasks.md, or unrelated configuration behavior. Do not
commit.
