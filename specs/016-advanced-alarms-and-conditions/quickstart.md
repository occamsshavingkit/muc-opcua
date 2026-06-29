# Quickstart: Advanced Alarms & Conditions

1. **Enable the Service**: Add `MICRO_OPCUA_SERVICE_ALARMS_CONDITIONS=1` to your build flags.
2. **Configure Storage**: Ensure `MU_MAX_CONDITIONS` is defined in your `config.h` (defaults to 10).
3. **Trigger an Alarm**:
   ```c
   mu_condition_id_t my_alarm = { /* ... */ };
   mu_alarms_set_active(&server, &my_alarm, true);
   ```
   This will automatically fire an event if `ActiveState` changed.
4. **Respond to Dialog**: When a client calls the `Respond` method, the server will process it via `mu_alarms_conditions_method_dispatch` and update the condition state.
