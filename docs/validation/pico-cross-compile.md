# Pico Cross-Compile Validation

- Architecture: ARM Cortex-M0+ (RP2040)
- Toolchain: `arm-none-eabi-gcc` 13.2.1 from `/usr/bin/arm-none-eabi-gcc`.
- Pico SDK: fetched by CMake with `PICO_SDK_FETCH_FROM_GIT=ON` into
  `build/us4-pico/_deps/pico_sdk-src`.
- Local configure command:
  `cmake -S . -B build/us4-pico -DMUC_OPCUA_PLATFORM=pico -DPICO_SDK_FETCH_FROM_GIT=ON -DMUC_OPCUA_PROFILE=embedded -DMU_MAX_SUBSCRIPTIONS=2 -DMU_MAX_MONITORED_ITEMS=100 -DMU_MAX_PUBLISH_REQUESTS=5 -DMU_MONITORED_QUEUE_DEPTH=2 -DMU_MAX_TRIGGER_LINKS=4 -DMUC_OPCUA_BUILD_EXAMPLES=ON`
- Local build command:
  `cmake --build build/us4-pico`
- Result: passed. The Pico SDK configured for `PICO_PLATFORM=rp2040`,
  `PICO_BOARD=pico`, and `PICO_GCC_TRIPLE=arm-none-eabi`; `muc_opcua`,
  `muc_opcua_pico`, and `pico_minimal_server` built.
- Build output:
  - `build/us4-pico/src/libmuc_opcua.a`
  - `build/us4-pico/platform/pico/pico_minimal_server.elf`
  - `build/us4-pico/platform/pico/pico_minimal_server.uf2`
- Additional freestanding smoke:
  `arm-none-eabi-gcc -ffreestanding -ffunction-sections -fdata-sections -std=c11`
  compiled every portable `src/**/*.c` file with the embedded profile defines.
