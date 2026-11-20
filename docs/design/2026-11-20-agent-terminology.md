# Agent terminology in Lab documentation

Status: implemented

[Design index](README.md)

## Problem and goal

The development process and quality roadmap name individual coding agents where the guidance applies to agents generally.

## Design

Describe the shared skill directory and optional formatting hook in generic terms. Keep the documented symlink paths.

## Test plan

Run the Lab documentation checker and check the diff for whitespace errors.

## Verification and review

The Lab documentation checker and `git diff --check` passed. The edits retain the existing skill paths and formatting command.

## Pull request

**docs(lab): generalize coding agent references**

Use generic coding agent terminology in the development process and quality roadmap. Keep the existing skill paths and optional formatting behavior documented.

See the design note in `docs/design/2026-11-20-agent-terminology.md`. Verified with `python3 external/Kinetra/tools/check_docs.py` and `git diff --check`.
