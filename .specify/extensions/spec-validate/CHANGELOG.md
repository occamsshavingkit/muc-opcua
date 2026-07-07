# Changelog

All notable changes to the Spec Validate extension will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-04-20

### Fixed

- Correct `repository` URL in `extension.yml` (was pointing at non-existent `spec-kit/spec-kit-spec-validate`)
- Set `author` in `extension.yml` to "Ahmed Eltayeb"

## [1.0.0] - 2026-04-16

### Added

- `validate` command: staged-reveal spec comprehension validation with multiple-choice quizzes
- `validate-tasks` command: same validation mechanism for tasks.md
- `review` command: peer review workflow with approve/request-changes/notes
- `gate` command: hard gate before implementation enforcing validation and review policy
- `status` command: read-only approval state display
- `analytics` command: private and aggregated comprehension analytics
- Split state model: git-tracked approval state + local private analytics
- Deterministic-first distractor generation with AI fallback
- 24-hour review SLA with timeout self-approval path
- Maintainer override with mandatory recorded reason
- Solo developer mode (skip review entirely)
- Auto-pass for specs with 0 critical items
- Content-hash-based staleness detection (SHA-256)
- Cross-platform scripts (bash + PowerShell)
- Agent-adaptive UX (IDE vscode/askQuestions + CLI conversational)
