---
description: "Re-generate Squad agent definitions as your spec evolves"
---

# Squad Bridge: Generate

Re-read the current spec and regenerate Squad agent definitions, capabilities,
and routing rules to stay in sync with spec changes. Safe to run repeatedly —
existing agents are updated in place; agents no longer supported by the spec
are flagged (not deleted).

This command is also triggered by the `after_specify` hook.

## User Input

$ARGUMENTS

## Steps

1. **Verify `.squad/` exists** — if not, tell the user to run
   `/speckit.squad.init` first and stop.

2. **Read the spec** from the active spec directory under `specs/` (e.g.,
   `specs/001-<name>/spec.md`).

3. **Load bridge config** from `.specify/extensions/squad/squad-config.yml`
   if it exists, otherwise use extension defaults.

4. **Read existing agents** from `.squad/agents/` (each agent lives in
   `.squad/agents/{name}/charter.md`) so changes can be diffed rather than
   blindly overwritten.

5. **Analyze the spec** to extract technology domains, architectural concerns,
   and cross-cutting roles (same logic as `init`). If `$ARGUMENTS` names a
   specific domain, limit regeneration to that domain's agent.

6. **Diff against existing agents**:
   - **New domains** found in spec but no matching agent → create new agent
   - **Changed domains** (different prominence or tech stack) → update agent
     capabilities and model tier
   - **Removed domains** (in existing agents but absent from new spec) →
     set `status: inactive` and note in output (do NOT delete)

7. **Update `squad.config.ts`** at the project root (using `defineSquad()` with
   `defineTeam()`, `defineAgent()`, and `defineRouting()`) to reflect the new
   agent set, routing rules, and model tier assignments.

8. **Update `.squad/routing.md`** to add routing rules for any new agents and
   update patterns for changed agents.

9. **Print a diff summary**:

   ```
   Squad agents updated
     ✅ Added   : data-engineer (PostgreSQL/migrations — proficient)
     ✏️  Updated : backend-engineer (added GraphQL capability)
     ⚠️  Inactive: mobile-engineer (no longer in spec — set to inactive)
   
   Routing rules updated: 8 total (2 added, 1 modified)
   ```

## Notes

- `inactive` agents remain in `.squad/` and can be reactivated manually with
  `squad` commands if needed.
- If the spec has not changed since the last run, this command reports
  "No changes detected" and exits cleanly.
