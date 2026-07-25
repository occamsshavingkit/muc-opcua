# T042 context digest

Modify `tests/unit/test_certificate_manager.c` and the receipt. If the required
RED run proves that the public Browse path cannot resolve an existing base node,
the core agent may apply only the minimal lookup correction in
`src/address_space/node_id.c`; no other production source is in scope.

Include `../../src/services/browse.h`. Add a helper that builds one forward
`mu_browse_description_t`, calls `mu_browse_process(NULL, NULL, ...)`, verifies
service/result status, and returns a bounded `mu_browse_result_t`. Add a helper
that proves a returned reference has the expected reference type, `is_forward`,
numeric namespace-0 target, and fails clearly if absent.

`test_certificate_manager_browse_types` must prove, through the Browse API:

- HasSubtype(45): 58→12555, 58→12556, 58→15594.
- HasSubtype(45): 12556→12557, 12556→12558, 12556→15017.
- HasSubtype(45): 12557→12559, 12557→15421.

Ground these assertions in OPC-10000-12 §7.8.4.1-§7.8.4.9 and §7.9.2.

`test_certificate_manager_browse_instances` must prove:

- Organizes(35): 85→15624.
- Organizes(35) and HasComponent(47): 15624→15625, 15626, 15627.
- HasOrderedComponent(49): 15625→12557, 12559, 15421; 15626→12558;
  15627→15017.

Ground these assertions in OPC-10000-12 §7.8.3.1 and §7.9.2. Register both
tests in `main()` under the existing Pull gate. Do not inspect private arrays,
alter other production code, edit tasks.md, or commit.
