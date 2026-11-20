# Local verification runs only graphics

Status: implemented · Branch: `docs/verify-levels` · Related: [Kinetra design](https://github.com/SMB-M87/Kinetra/blob/main/docs/design/2026-11-20-verify-locally-graphics.md), [verification in Lab](../../AGENTS.md#verification-in-lab)

Lab's Full level ran `ci` and `headless` locally, and `windows` for Windows-related changes, although the pull request's CI runs `ci`, `headless` and a native Windows build on every push, and again after the merge. Locally it now runs only the graphics workflow and the smoke runs; the pull request's checks do the rest. The shared rule and its reasoning are in the [Kinetra design](https://github.com/SMB-M87/Kinetra/blob/main/docs/design/2026-11-20-verify-locally-graphics.md); this document covers Lab's `AGENTS.md`.

## Table of contents

- [Decisions](#decisions)
- [Affected areas](#affected-areas)
- [Verification](#verification)
- [Pull request](#pull-request)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Local Full level | `cmake --workflow --preset graphics` and the README smoke runs | User's choice: `ci` and `headless` already run in the pipeline, twice |
| When verified | Local graphics passed and the pull request's checks are green | The CI half is no longer run locally |
| `cmake --workflow --preset windows` | Locally only for the WSL cross build itself (`cmake/toolchains/`, the `windows` preset) | CI's `windows` job builds natively with the same compiler family but never runs the cross build |

## Affected areas

[AGENTS.md](../../AGENTS.md): the Full row of the level table and the Windows row of the mapping table.

## Verification

`python3 external/Kinetra/tools/check_docs.py`: all links resolve.

## Pull request

**docs(agents): run only graphics locally and leave the rest to CI**

The Full level in Lab now runs `cmake --workflow --preset graphics` and the smoke runs locally; format, docs, `headless`, `ci` and the native Windows build come from the pull request's checks, and a change is verified when both pass. The local `windows` workflow remains only for changes to the WSL cross build, which CI does not run. Docs only.
