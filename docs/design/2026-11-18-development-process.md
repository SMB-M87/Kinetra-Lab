# Development process and agent rules (Lab)

Status: implemented · Branch: `docs/development-process` · Related: [Kinetra design document](../../external/Kinetra/docs/design/2026-11-18-development-process.md), [quality roadmap](2026-11-18-quality-roadmap.md)

Lab adopts the development cycle, verification levels and skills that are defined once in Kinetra, adds its Lab-specific rules, and corrects documentation that no longer matched the launcher code.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

- **Weak agent rules.** Lab's `AGENTS.md` (25 lines) contained only a build requirement and a copied documentation section. It required a full rebuild after every change, documentation included.
- **Docs that contradict the code.** The architecture page and both launcher READMEs claimed that `kinetra::SessionHost` owns sessions. In reality, `launcher::run` ([launcher/src/session.cpp](../../launcher/src/session.cpp)) runs its own loop with a `std::unique_ptr<lab::Session>`.
- **Unprotected private material.** The local reference folders `files/` and `bms_files/` sat untracked in a public repository, one `git add -A` away from being published.

## Goals and non-goals

**Goals:**
- Lab rules that add to Kinetra's instead of copying them.
- A Lab test mapping for the Targeted level.
- Skills available in Lab.
- Correct session-ownership documentation.
- A roadmap for the clean-up.

**Non-goals:**
- Changing the launcher code. Switching to `SessionHost` is roadmap phase 3.

## Decisions

The decisions are recorded in the [Kinetra design document](../../external/Kinetra/docs/design/2026-11-18-development-process.md#decisions); the roadmap records the phase decisions. Lab-specific:

| Decision | Choice | Why |
| --- | --- | --- |
| Skills in Lab | Symlinks `.agents/skills` and `.claude/skills` to `external/Kinetra/.agents/skills` | One skill set for both repositories; Codex and Claude Code each find their own path |
| Drift in session ownership | Correct the docs to match the code now; change the code in a later phase | Documentation must describe implemented behavior |
| Local reference folders | Ignore `files/` and `bms_files/` in `.gitignore` | They hold machine-local global rules and work examples that must not be published |

## Affected areas

- `AGENTS.md`: rewritten (Lab-specific rules, test mapping, layers, submodule git).
- `.agents/skills`, `.claude/skills`: symlinks.
- `.gitignore`.
- `docs/architecture.md`: build diagram, session sequence and text.
- `launcher/src/README.md`, `launcher/include/README.md`: diagrams.
- `docs/README.md`, `README.md`: links to the process and design documents.
- `docs/design/`: this document, the roadmap and the index.
- `external/Kinetra`: pointer to the Kinetra branch with the new rules.

## Test plan

Documentation only, so the **Docs** level applies: `python3 external/Kinetra/tools/check_docs.py` passes, both symlinks resolve, and the corrected claims match `launcher/src/session.cpp` (checked by reading the loop).

## Review

- **Code against docs.** Checked every claim in the new sequence diagram against `launcher::run`: first update with zero time and empty input; `exit_requested` checked before `return_to_menu`; the session is reset on return; the direct executables use `kinetra::run_session`.
- **Test mapping.** Checked the target and test names in the mapping table against the `add_executable` and `add_test` calls in the CMake files.
- **Mermaid parsing.** Renamed the sequence participant `Loop` to `Host` to avoid the `loop` keyword.

## Verification

| Command | Result |
| --- | --- |
| `python3 external/Kinetra/tools/check_docs.py` | passed |
| `ls .agents/skills .claude/skills` | both list the six skills |
| Build | not run: documentation-only change (Docs level) |

## Pull request

**Title:** `docs: adopt Kinetra development process and fix launcher session docs`

**Description:**

> - `AGENTS.md` now adds Lab-specific rules (layers per application, Targeted test mapping, submodule git) on top of the shared rules in Kinetra.
> - Agent skills are available through `.agents/skills` and `.claude/skills` (symlinks into the Kinetra submodule).
> - Corrects the session-ownership docs: the bundled launcher runs its own `launcher::run` loop and does not use `SessionHost` yet.
> - Adds the quality roadmap (`docs/design/2026-11-18-quality-roadmap.md`) for tooling, tests and the per-module clean-up.
> - Ignores the local reference folders `files/` and `bms_files/`.
> - Updates `external/Kinetra` to the branch with the shared process docs. **Push Kinetra `docs/development-process` first.**
>
> Documentation only; verified with `python3 external/Kinetra/tools/check_docs.py`.

## Open points

- `labs/demo/README.md`, `labs/dotio/README.md` and `README.md` still mention Artix as the desktop target. That is a platform fact, not the removed layout reference, so it stays.
