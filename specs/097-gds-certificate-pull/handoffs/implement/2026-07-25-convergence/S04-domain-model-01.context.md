# T041 context digest

Modify only `src/address_space/base_nodes.c`:

- `s_objects_refs`: Organizes(35) forward 85→15624.
- `s_cert_groups_refs`: preserve inverse 15624→85, then add Organizes(35)
  and HasComponent(47) forward from 15624 to each of 15625, 15626, 15627.
- `s_default_app_group_refs`: preserve inverse Organizes to 15624, then add
  HasOrderedComponent(49) forward to 12557, 12559, and 15421.
- `s_default_https_group_refs`: preserve inverse Organizes to 15624, then add
  HasOrderedComponent(49) forward to 12558.
- `s_default_user_group_refs`: preserve inverse Organizes to 15624, then add
  HasOrderedComponent(49) forward to 15017.

Use the existing Pull + Base Info Type Information gate for group arrays and the
same Pull/type-information gate in `s_objects_refs`. Cite
OPC-10000-12 §7.8.3.1 and §7.9.2. Existing node definitions are already under
the outer Pull gate; there is no unresolved gate gap. Keep `s_objects_refs`
initializer syntax valid when the type-system facet is off. Do not alter T040 edges,
tests, methods, headers, Kconfig, CMake, or tasks.md. Do not commit.
