# Quickstart: Verify OPC UA Conformance and Size Repairs

1. Configure and run the host test build:

   ```sh
   cmake -S . -B build/019-tests -DMICRO_OPCUA_BUILD_TESTS=ON -DMICRO_OPCUA_OPTIMIZE_SIZE=ON -DMICRO_OPCUA_STACK_USAGE=ON -DCMAKE_BUILD_TYPE=Debug
   cmake --build build/019-tests -j
   ctest --test-dir build/019-tests --output-on-failure
   ```

2. Measure embedded profile sizes:

   ```sh
   scripts/measure_size.sh all
   ```

3. Verify the protocol repairs covered by this feature:

   - StatusCode constants match OPC-10000-4 §7.38.2 and `StatusCode.csv`.
   - AggregateFilter binary encoding uses `ns=0;i=730` per `NodeIds.csv` and OPC-10000-4 §7.22.4.
   - Average, Minimum, and Maximum use `ns=0;i=2342`, `ns=0;i=2346`, and `ns=0;i=2347` per OPC-10000-13 §4.2.2.4, §4.2.2.9, and §4.2.2.10.
   - `GetEndpoints` filters non-empty `profileUris` per OPC-10000-4 §5.5.4.2.
   - Traceability docs and `tasks.md` cite exact OPC UA sections for every protocol task.
