---
description: "Initialize a Squad team from the current Speckit spec"
---

# Squad Bridge: Init

Read the current project spec and bootstrap a Squad team tailored to its
technology domains, roles, and work types. Run this once after your initial
`/speckit.specify` to get a squad that mirrors your project's shape.

## Prerequisites

Verify Squad CLI is available:

```bash
squad --version
```

If that fails, install it first:

```bash
npm install -g @bradygaster/squad-cli
```

## User Input

$ARGUMENTS

## Steps

1. **Read the spec** from the active spec directory under `specs/` (e.g.,
   `specs/001-<name>/spec.md`). If no spec directory exists, tell the user
   to run `/speckit.specify` first and stop.

2. **Read tasks** from `specs/<id>/tasks.md` if it exists (used to infer work
   types and routing signals).

3. **Load bridge config** from `.specify/extensions/squad/squad-config.yml`
   if it exists, otherwise use extension defaults.

4. **Analyze the spec** to extract:
   - Technology domains (e.g., React, Node.js, PostgreSQL, Python, Go, iOS)
   - Architectural concerns (e.g., API design, database schema, DevOps/CI)
   - Cross-cutting concerns (e.g., auth, testing, documentation)
   - Any explicit roles or team structure mentioned in the spec

5. **Initialize Squad** if `.squad/` does not already exist:

   ```bash
   squad init
   ```

6. **Generate agent definitions** — for each identified domain/concern,
   create a Squad agent with:
   - A descriptive `name` (e.g., `backend-engineer`, `frontend-engineer`)
   - A `role` derived from the domain
   - `capabilities` array (name + level: expert/proficient/basic) inferred
     from how prominently the domain features in the spec
   - `model` set to the tier from config that matches the agent's complexity
   - `status: active`

   Write each agent as a `.squad/agents/{name}/charter.md` file following
   Squad's format. Also generate or update `squad.config.ts` at the project
   root using `@bradygaster/squad-sdk`'s `defineSquad()` (with `defineTeam()`,
   `defineAgent()`, and `defineRouting()` sub-builders) covering all agents,
   routing rules, and model tier settings from config.

7. **Generate routing rules** in `.squad/routing.md` that map task keywords
   and domain patterns to the agents created above. Examples:
   - `/\bAPI|endpoint|REST|GraphQL\b/i` → backend-engineer
   - `/\bReact|component|UI|frontend\b/i` → frontend-engineer
   - `/\btest|spec|coverage|QA\b/i` → qa-engineer

8. **Print a summary**:

   ```
   ✅ Squad initialized
      Agents created : 3
        - backend-engineer   (Node.js/REST API — expert)
        - frontend-engineer  (React/TypeScript — expert)
        - qa-engineer        (Testing/QA — proficient)
      Routing rules  : 6
      Config         : squad.config.ts
   
   Next steps:
     squad doctor          — verify your team
     /speckit.plan         — create your implementation plan
     /speckit.tasks        — generate tasks from the plan
     /speckit.squad.route  — route tasks to agents (after tasks exist)
   ```

## Notes

- Running this command more than once is safe — it will not overwrite existing
  agent files. Use `/speckit.squad.generate` to refresh agents as the spec
  evolves.
- If `$ARGUMENTS` contains a domain or role name, generate an agent for that
  domain in addition to those inferred from the spec.
