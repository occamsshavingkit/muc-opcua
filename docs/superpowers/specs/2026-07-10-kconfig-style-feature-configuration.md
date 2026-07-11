# Kconfig-style feature configuration — plan for next sprint

**Status:** IN PROGRESS (2026-07-11). Execution plan: `~/.claude/plans/calm-dreaming-lynx.md`.

## Implementation status (2026-07-11)

Decisions updated from this doc after execution began:
- **Tool = kconfiglib (pure Python)**, not the vendored kernel-C sources. Justification: Python
  is already a build/CI dep here (`check_claim_map.py`, `measure_size.sh`), so the kernel-C
  "no Python" advantage buys nothing, while kconfiglib is pip-installable / vendorable with zero
  ncurses/GPL-vendoring overhead. (This doc's designated fallback becomes the primary.)
- **Gates at the CU/feature leaf**, OPC Part/§/paragraph citations as help-text metadata
  (as this doc already specified) — *not* paragraph-level gates.
- **Sequencing = after Project B** (B1–B7 all merged) — the doc's open sequencing question is moot.
- **ALLOW_HEAP, platform flags (MDNS/HAVE_*/IS_*), build knobs stay CMake-derived** — not in
  Kconfig (memory-model / platform / tooling, not togglable facets).

**Phase 0 (spike) — DONE, all claims confirmed against this codebase:**
- The `#ifdef`-preservation open question is **RESOLVED**: with bare Kconfig symbol names +
  `CONFIG_=MUC_OPCUA_`, `genconfig`/`write_autoconf` emits `#define MUC_OPCUA_<X> 1` for `y` and
  leaves it **undefined** for `n` — `#ifdef`-safe, macro names kept verbatim, **no `#ifdef`-site
  rewrite**. `.config`/defconfig lines read `MUC_OPCUA_<X>=y`.
- `depends on` enforces hard dependencies (a defconfig forcing `DATA_ACCESS=y` without
  `BASE_NODES` is refused — `n` in output).
- A single generator (`scripts/kconfig/gen_config.py`) emits both `autoconf.h` (compiler) and a
  `config.cmake` (`set(MUC_OPCUA_X ON/OFF)` for source selection).

**Phase 1 (tree) — IN PROGRESS.** The full Kconfig tree (`/Kconfig`, 43 symbols: profile choice +
~37 feature facets + STANDARD_PROFILE marker + CLIENT_NANO) is authored with per-child
`default y if PROFILE_*` (derived from the current per-profile flag matrix) and every dependency
edge (the 11 `FEATURE_DEPENDENCIES` + the 2 divergent edges + the implicit `SESSION_TIMEOUT`
coupling) as `depends on`. `configs/<profile>.defconfig` (×6) select the choice.
**Acid test passes:** `scripts/kconfig/check_baseline.py` confirms the Kconfig-resolved feature
set == today's CMake-resolved set for all six profiles (byte-identical, excluding the
CMake-derived ALLOW_HEAP/MDNS and the behavior-neutral unread `PROFILE_CUSTOM`).
**Remaining Phase 1:** wire CMake to consume `gen_config.py` output (include `autoconf.h`, source
selection via `config.cmake`), then retire the `option()`/`apply_profile_option`/reset-loop +
`FEATURE_DEPENDENCIES` machinery and the `features.h` dependency `#error`s; re-run the full build
matrix for behavior-neutrality.

---

**Original plan (planning only, no code written) follows.** This document captured the decision
and design before implementation started.

## Problem

`docs/build-and-gating.md` (merged 2026-07-10, PR #283) fixed the *mechanics* of
subtracting/adding a feature from a named profile (`-DMUC_OPCUA_PROFILE=standard
-DMUC_OPCUA_SECURITY=OFF`), and promoted every cross-feature dependency to a
configure-time `FATAL_ERROR`. It did not fix the *discoverability* problem: ~30
`MUC_OPCUA_*` flags are a flat CLI/CMakeCache surface, with no way to browse "what
does the Data Access facet turn on", no live view of what a candidate override would
break, and no single place that shows the tree shape.

The actual shape of the config space (confirmed in conversation, 2026-07-10): it is
**one containment tree**, not several competing taxonomies —

```
Server
  Profile (nano / micro / embedded / standard / full)      -- soft preset
    Facet (Data Access, Method Server, Auditing, ...)       -- hard container
      Section / Feature (individual flags, capacity knobs)  -- leaf
Client
  Profile (none / nano / standard)
    ...
```

— additive (`standard` ⊇ `embedded` ⊇ `micro` ⊇ `nano`), and every OPC-10000-Part/§
citation is metadata *on* a node in this tree (help text), not a second physical tree.

## Tool survey (2026-07-10 research)

| Option | TUI/GUI | Extra runtime dep | Fits the containment tree? | Verdict |
|---|---|---|---|---|
| **Kconfig + Kconfiglib** (Zephyr/ESP-IDF's choice) | `menuconfig.py` (curses) + `guiconfig.py` (Tk), both ship with the library | Python 3, build-time only | Yes — natively: `depends on` = hard container, `default y if PROFILE_X` = soft preset (see [Design](#design-mapping)) | Candidate if Python-as-build-dep is acceptable |
| **Kconfig, vendoring the Linux kernel's own `scripts/kconfig/` C sources** (`mconf.c`/`nconf.c`/`qconf.cc`/`gconf.c`/`conf.c`) | `mconf`/`nconf` (curses, C), `qconf`/`gconf` (Qt/GTK GUI) | None at runtime; a C toolchain to build the tool once, same as any other host build tool | Same Kconfig language, same tree semantics | **Primary candidate** — see correction below |
| ~~Kconfig + `kconfig-frontends`~~ (the standalone third-party package) | Same C tools as above | None at runtime | Same | **Rejected** — no upstream maintainer; even Espressif's own fork (for ESP-IDF) hasn't released since 2019. Initially described this as "the kernel's own tooling," which was wrong — it's a separate extraction, not something the kernel itself depends on |
| **CMakePresets.json** (CMake-native, 3.19+) | IDE preset pickers (VS Code, CLion, Visual Studio) render it as a dropdown out of the box | None — ships with CMake | No leaf-level dependency-aware browsing; presets are flat named bundles of `-D` values with `inherits` | Good **complement**, not a replacement — see [Recommendation](#recommendation) |
| `ccmake` / `cmake-gui` | Yes, ships with CMake | None | No — flat alphabetical list of every CACHE var, no nesting/hide-on-dependency (unless paired with `cmake_dependent_option()`, which only hides single-level dependents) | Already available today at zero cost; too weak alone for the tree the user wants |
| `c_cpp_project_framework` (GitHub, Kconfiglib+CMake glue) | `menuconfig` via `project.py` | Python | Only inside its own opinionated component-per-directory restructuring | **Rejected** — would force restructuring the whole repo around its component model; not a drop-in for an existing single-library project |

No fundamentally different, comparably mature tool exists outside the Kconfig family
for "hierarchical, dependency-aware, TUI-browsable build-time feature selection in
C" — Linux, Zephyr, ESP-IDF, U-Boot, Buildroot, BusyBox, coreboot all converge on it.

**Correction (caught in review):** the Linux kernel's *own* Kconfig tooling
(`scripts/kconfig/mconf.c` et al., in-tree) is pure C and has never used Python —
that instinct was right. But the standalone `kconfig-frontends` package is a
*separate*, third-party extraction of that code for non-kernel projects to depend
on externally, and it turns out to be effectively unmaintained. The two most
comparable real precedents — **U-Boot and Buildroot**, both embedded-C projects
that adopted Kconfig with zero Python — do **not** depend on `kconfig-frontends`
at all; both vendor the kernel's `scripts/kconfig/` C sources directly into their
own tree. That's the current, actually-used pattern for "Kconfig with no Python",
not the abandoned external package.

## Recommendation

**Kconfig, with the tooling vendored from the Linux kernel's own `scripts/kconfig/`
C sources** (not the abandoned `kconfig-frontends` package, not Kconfiglib/Python)
as the primary mechanism — flags, dependencies, and the `mconf`/`nconf` TUI /
`qconf`/`gconf` GUI. This is the U-Boot/Buildroot-precedent path: zero Python in the
toolchain, tracking a maintained upstream (the kernel tree itself) instead of a
third-party fork nobody maintains.

Add **`CMakePresets.json` as a thin, optional convenience layer** on top for IDE
users who just want a "pick Standard" dropdown without ever opening menuconfig — a
preset can simply point at a starting Kconfig `defconfig`. Not competing with
Kconfig; near-zero extra cost, solves a different problem (IDE-visible picker) than
Kconfig solves (tree browsing + dependency-aware editing).

Kconfiglib (Python) remains the fallback if vendoring the kernel's C sources proves
awkward in practice (e.g. licensing friction even as a build-time-only host tool,
or the vendored code drifting out of sync with upstream Kconfig language changes) —
see open questions.

## Design mapping

Established in conversation and confirmed against Kconfig semantics:

- **`depends on`** = hard containment. A section/feature cannot be enabled — is not
  even selectable in menuconfig — without its facet. Maps 1:1 onto the *existing*
  `MUC_OPCUA_FEATURE_DEPENDENCIES` table / `features.h` `#error`s; this migration
  absorbs and replaces both.
- **`default y if PROFILE_STANDARD`** (not `select`!) = the soft preset a profile
  applies. Stays user-togglable and "sticky" once touched — Kconfig's native
  equivalent of the `_LAST_FORCED`-comparison trick hand-built in CMake this
  session (PR #283). `select` is deliberately avoided: it force-locks a symbol and
  is exactly the pitfall Kconfig's own docs warn about (a selector doesn't verify
  the selected symbol's own dependencies).
- **Author direction: per-child, not per-parent.** Give each facet its own
  `default y if PROFILE_MICRO || PROFILE_EMBEDDED || PROFILE_STANDARD || PROFILE_FULL`
  list (which profiles want *me*), rather than each profile listing every facet it
  wants (today's CMake pattern, which duplicates/drifts — `standard`'s block already
  repeats `embedded`'s). The additive nano⊆micro⊆embedded⊆standard chain falls out
  automatically instead of being hand-maintained per profile block.
- **Profiles are `defconfig` presets**, not container nodes — a starting `.config`
  layered under the facet tree, matching Zephyr's board-default pattern. This also
  directly fixes the discoverability gap: menuconfig loaded from a profile's
  defconfig shows, live, which facets are already on and which further options are
  now reachable — no separate doc lookup needed to learn e.g. "Data Access needs
  Base Nodes".
- **Client/Server** = a top-level `menu` split, matching the existing
  `MUC_OPCUA_CLIENT_PROFILE`/`MUC_OPCUA_PROFILE` separation in the code today.
- **C header generation preserves existing `#ifdef` gating** (needs Phase-0
  verification, not yet confirmed against this codebase specifically): the kconfig
  C tooling's standard `autoconf.h` generation emits `#define CONFIG_FOO 1` for `y`
  and leaves `FOO` **undefined** for `n` — `#ifdef`-safe, matching every existing
  `#ifdef MUC_OPCUA_X` gate in ~100 C files today (this behavior is identical
  whether generated by the vendored kernel C tooling or by Kconfiglib's
  `genconfig.py`, should the fallback be needed). Whichever path is used, confirm
  the macro-prefix mechanism lets the existing `MUC_OPCUA_` names be kept verbatim
  rather than migrating every call site to a `CONFIG_` prefix. **If this holds, the
  bulk of the migration is the flag-declaration layer + CMake glue + docs — not a
  rewrite of every `#ifdef` site in the codebase.** Must be spiked and confirmed
  before committing to a full-scope estimate.

## Phased plan

1. **Phase 0 — spike (small, time-boxed).** Vendor the Linux kernel's
   `scripts/kconfig/` C sources (pin to a specific kernel tag; note the license and
   confirm build-time-only host-tool use is an acceptable posture). Hand-write a
   `Kconfig` skeleton for ~10 representative symbols (one profile choice, one
   hard-dependency pair mirroring `DATA_ACCESS`/`BASE_NODES`, the client/server menu
   split) plus a minimal CMake shim that builds `mconf`/`nconf` and consumes the
   generated output. Confirm the `#ifdef`-preserving header generation claim above
   against this codebase's actual macro names. Deliverable: a working `mconf`/
   `nconf` TUI a reviewer can click through, not yet wired into the real build.
2. **Phase 1 — port the flag/dependency layer.** Translate all ~30
   `MUC_OPCUA_PROFILE_CONTROLLED_OPTIONS` + the independent flags (§ full list in
   `docs/build-and-gating.md`) into `Kconfig` files, one per subsystem area
   (security, services, facets, client), organized as the containment tree above.
   `MUC_OPCUA_FEATURE_DEPENDENCIES` and `features.h`'s dependency `#error`s are
   retired (superseded by native `depends on`).
3. **Phase 2 — capacities.** Decide and implement whether `MU_MAX_*` capacity knobs
   (`capacities.h`'s default<profile<user cascade) move into Kconfig as `int`
   symbols with per-profile `default`, or stay a separate `-D` layer on top of a
   Kconfig-selected feature set. (Leaning: move in — they're part of the same
   "confusing possibilities" surface the user raised, and Kconfig `int` symbols with
   `range`/`default...if` handle exactly this cleanly.)
4. **Phase 3 — profiles as defconfigs, tooling, CI.** Author `nano.defconfig`
   through `full.defconfig`; wire `mconf`/`nconf`/`savedefconfig` (built from the
   vendored kernel `scripts/kconfig/` sources) as CMake custom targets; update the
   `Makefile` convenience targets, all five GitHub Actions jobs,
   `scripts/test_profile_gating.sh` (becomes a Kconfig-output assertion script),
   and every doc this session touched (`docs/build-and-gating.md` becomes largely
   auto-generated from `Kconfig` help text rather than hand-maintained).
5. **Phase 4 — cutover.** Remove the CMake-native flag declarations
   (`option()`/`set(... CACHE BOOL ... FORCE)` blocks) once Kconfig is the source of
   truth; keep `CMakeLists.txt` consuming the generated header/defines only.

## Open questions (resolve before Phase 1 starts)

- **Vendoring posture.** Which kernel version/tag to pin `scripts/kconfig/` to, how
  to track upstream changes (git subtree vs. a periodic manual sync), and confirm
  the GPL-2.0 licensing is fine for a build-time-only host tool that never links
  into the shipped library (the same posture U-Boot/Buildroot already rely on, but
  worth a project-specific sign-off, not an assumption).
- **Python fallback trigger.** If vendoring turns out to be awkward in practice
  (build friction on a target CI/dev environment, or the vendored C tool needing
  libraries — ncurses/Qt/GTK — not available in some contributor's toolchain), fall
  back to Kconfiglib (Python, build-time only) — same Kconfig-language design,
  different frontend implementation, no change to the `Kconfig` files themselves.
- **`#ifdef`-preservation claim** (see Design mapping) — must be spiked, not
  assumed, before estimating Phase 1/2 scope.
- **Does `CMakePresets.json` get built at all**, or is it deferred/dropped given
  Kconfig's defconfig + menuconfig already covers the "pick a profile" need? Lean
  toward deferring it — it's additive and low-cost to add later, not a blocker for
  the core migration.
- **Sequencing against Project B** (Part 7 Server Facets, specs 060+, per
  [[profile-modernization-roadmap-progress]]): does this migration happen *before*
  060 (so new facets are authored Kconfig-native from the start) or *after* (so it
  doesn't block/interfere with in-flight facet work)? Recommend before — every new
  facet spec from 060 onward would otherwise need re-authoring into Kconfig shortly
  after landing in CMake.

## Non-goals for this sprint

- No code changes yet. This document is the artifact for this sprint.
- Not deciding the exact `Kconfig` file layout/naming (subsystem-per-file vs.
  monolithic) — that's a Phase-0/1 detail, not a planning-level decision.
