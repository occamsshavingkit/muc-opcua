# Data Model: Advanced Alarms & Conditions

## `mu_condition_t`
Tracks the state of a single condition.
- `condition_id` (NodeId): Identifier for the condition instance.
- `active_state` (bool): True if alarm is active.
- `acked_state` (bool): True if alarm has been acknowledged.
- `confirmed_state` (bool): True if alarm requires/has received confirmation.
- `retain` (bool): True if the condition is still relevant and should be surfaced to late-joining clients.

## `mu_dialog_condition_t` (extends condition)
- `expected_response` (int): Identifier for the selected response.
- `valid_responses` (array/bitmask): Masks what responses are allowed.

## `mu_server_t` Additions
- `mu_condition_t conditions[MU_MAX_CONDITIONS]`: Statically allocated array of state trackers.
- `size_t condition_count`: Currently tracked conditions.
