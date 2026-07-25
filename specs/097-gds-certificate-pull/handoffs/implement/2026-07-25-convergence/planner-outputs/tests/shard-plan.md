# Test vertical shard plan

## S05-test-validation-01 — T042

- Add public Browse-path regression tests for the T040 type hierarchy and T041
  instance hierarchy.
- Assert reference type, forward direction, and numeric target NodeId through
  `mu_browse_process`; do not inspect private reference arrays directly.
- Ground type assertions in OPC-10000-12 §7.8.4 and instance assertions in
  §7.8.3.1 and §7.9.2.
