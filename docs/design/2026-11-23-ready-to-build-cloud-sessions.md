# Ready-to-build cloud sessions

Status: implemented · Branch: `ccr-d6ebceac-13bzfb` (Lab), `build/cloud-session-setup` (Kinetra) · Related: [roadmap](../roadmap.md), [Kinetra tools](../../external/Kinetra/tools/README.md#cloud_setupsh)

A cloud session of Kinetra-Lab or Kinetra starts without SDL2, nlohmann JSON, ccache and CI's clang-format, and in Lab without the Kinetra submodule, so every code task installs them first. A SessionStart hook in each repository now runs one setup script at the start of every cloud session, so a session builds, tests and checks formatting as CI does at once.

## Table of contents

- [Problem](#problem)
- [Goals and non-goals](#goals-and-non-goals)
- [Decisions](#decisions)
- [Design](#design)
- [Affected areas](#affected-areas)
- [Test plan](#test-plan)
- [Review](#review)
- [Verification](#verification)
- [Pull request](#pull-request)
- [Open points](#open-points)

[Directory guide](README.md) · [Project overview](../../README.md) · [Documentation index](../README.md)

## Problem

Measured on 2026-11-23 in this branch's own cloud session (Ubuntu 24.04.5, the *Default* environment):

- **Packages.** `libsdl2-dev`, `nlohmann-json3-dev`, `ccache` and `libgl-dev` were missing, so `cmake --preset dev` could not configure. `clang-format` was Ubuntu's 18.1.3; CI formats with 23.1.5, the version of its pinned Arch snapshot ([Kinetra's ci.yml](../../external/Kinetra/.github/workflows/ci.yml)), so a session could not reproduce the format check.
- **Submodule.** `git submodule status` showed `-bc17b4f external/Kinetra`: the checkout had no engine sources. `.claude/skills` links into the submodule, so the repository skills (`feature`, `roadmap`, `verify` and the rest) were missing too until the session ran `git submodule update --init --recursive`.
- **The environment does not hold a setup script.** On 2026-11-22 the *Linux beyond Arch* session created a cloud environment *Kinetra-Lab* whose setup script installed the packages; on 2026-11-23 the account lists only *Default*, and that day's i4sim session (#92) and this one installed packages by hand. A setup script lives in the environment's settings, outside both repositories: nobody reviews it, and a session in another environment does not get it.
- **Where clang-format 23 comes from.** Under the default *Limited* network access, `apt.llvm.org` and `archive.archlinux.org` do not answer; `pypi.org` does, and PyPI's `clang-format` package has 23.1.5. `uv`, which the cloud image includes, installs it in under a second.
- **Cost.** `apt-get update` took 2 s and installing the missing packages (52 downloads, `--no-install-recommends`, as CI's Ubuntu run installs them) 6 s. With them and clang-format 23.1.5, CI's format command passes on both repositories, `cmake --preset dev` configures, and `kinetra_lab_tests` builds and passes.

## Goals and non-goals

Goals:

- Every Claude Code cloud session of Kinetra-Lab or Kinetra starts with the Debian and Ubuntu package set of the README and [development](../../external/Kinetra/docs/development.md#dependencies-and-options) (Lab adds nlohmann JSON), clang-format 23.1.5 first on `PATH`, and, in Lab, the Kinetra submodule checked out.
- A resumed session, or one that already has everything, spends well under a second on it.
- A failed install does not block the session; the agent learns what is missing and how to install it.
- The package set and the clang-format version are written once for both repositories.

Non-goals:

- The environment's setup script: the user chose the hook (below). An environment script can still call the same script later, for caching.
- Sessions with several repositories and threads of a project: Claude Code loads no repository hooks there ([cloud environments](https://code.claude.com/docs/en/cloud-environments#limitations-in-cloud-sessions)).
- Local machines: the script does nothing outside a cloud session. Developers keep installing from the README.
- Arch parity beyond clang-format: a session builds with Ubuntu's GCC 13, so CI's `linux (arch)` run stays the reference for warnings, as on Ubuntu CI.
- The Windows cross toolchain: Windows is verified by CI's `windows` job.

## Decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Mechanism | A SessionStart hook in each repository's `.claude/settings.json`, which runs a script under `tools/` | The user's choice on 2026-11-23: the hook is versioned and reviewed, works in every environment, *Default* included, and needs no step in the settings. The environment's setup script, which Claude Code caches, lives outside the repositories and was lost once |
| Repositories | Kinetra and Kinetra-Lab in this change | The user's choice on 2026-11-23 |
| One script | Kinetra's `tools/cloud_setup.sh` holds the package set and the clang-format version and takes extra packages as arguments; Lab's `tools/cloud_setup.sh` loads the submodule and calls it with `nlohmann-json3-dev` | One copy of the set and the version for both repositories. Lab needs a script of its own because Kinetra's does not exist until the submodule is loaded |
| clang-format | `uv tool install clang-format==23.1.5` from PyPI, into uv's tool directory (`~/.local/bin` by default), only when neither the clang-format that `PATH` finds nor uv's copy is that version | The exact CI version, from a host the default network access allows; LLVM's apt repository is not reachable. `uv` is in the cloud image. A resumed session reuses its copy instead of downloading it again |
| Only in cloud sessions | The script exits at once unless `CLAUDE_CODE_REMOTE` is `true` | Hooks run in local sessions too; installing system packages on a developer's machine is not the hook's business |
| Synchronous | The hook runs before the session takes its first message, on `startup` and `resume` | No race with the first build. A first start costs seconds; a resumed VM keeps its files, and one that was reclaimed needs the install again |
| Failure | The script always exits 0 and prints one status line | Claude Code adds a SessionStart hook's output to the agent's context only when it exits 0; a failure should inform the agent, not block the session |
| Submodule | Loaded only when `external/Kinetra/.git` is missing | On a resumed session the agent may have moved the submodule on purpose; `git submodule update` would reset it |
| Time limits | The submodule fetch gets 3 minutes, `apt-get update` 2, the package downloads 5 and the uv install 2, each under `timeout`; the install from the downloaded files has none; the hook's own limit is 900 s | A stalled mirror would otherwise hold the session start until the hook's limit, and a killed hook prints nothing; CI once saw `archive.ubuntu.com` still downloading after an hour. Stopping dpkg halfway would break the next run. The limits add up to 720 s and leave the install time within 900 s |
| Version copies | The comments of both `ci.yml` files that say how to bump the Arch snapshot name the script's clang-format version; Lab's Ubuntu step names the script as a copy of the package set | The script's pin must follow the snapshot, and the person who bumps it reads those comments |
| Index errors | `apt-get update -o APT::Update::Error-Mode=any`, as in CI | `apt-get update` succeeds when an index fails to download; the failure then names the update, not a package apt could not find |

## Design

```mermaid
flowchart TD
    H[SessionStart hook: startup or resume] --> R{CLAUDE_CODE_REMOTE is true?}
    R -->|no| X[Exit, nothing printed]
    R -->|yes| L{Lab: external/Kinetra/.git present?}
    L -->|no| S[git submodule update --init --recursive]
    L -->|yes| K
    S --> K[Kinetra's cloud_setup.sh with Lab's extra package]
    K --> P{Every package installed?}
    P -->|no| A[apt-get update and install the missing ones, under timeout]
    P -->|yes| C
    A --> C{clang-format is 23.1.5?}
    C -->|no| U[uv tool install clang-format==23.1.5]
    C -->|yes| O
    U --> O[Print one status line, exit 0]
```

The takeaway: a session that already has everything only runs the checks, and every path ends in one line for the agent.

- **Kinetra's script** checks each package with `dpkg-query` and installs only the missing ones, with `--no-install-recommends` like CI's Ubuntu run. It then compares `clang-format --version` with its pin; when they differ, it looks at the copy in uv's tool directory (`uv tool dir --bin`) and installs the pin with `uv` only when that copy is missing or another version. When `PATH` still finds another clang-format first, it puts uv's directory first for the session through `CLAUDE_ENV_FILE`, once, so the pinned clang-format wins over Ubuntu's. Output of apt and uv goes to a log file; standard output carries one line, such as `Cloud setup: installed libsdl2-dev ccache; clang-format 23.1.5 ready.` or `Cloud setup failed: … see <log>`.
- **Lab's script** loads the submodule when it is missing and then runs Kinetra's script with `nlohmann-json3-dev`. When the submodule cannot be loaded, or its commit has no `tools/cloud_setup.sh`, it prints that and what to do by hand.
- **The hooks** call `bash "$CLAUDE_PROJECT_DIR"/tools/cloud_setup.sh`, so they find the script from any working directory.

## Affected areas

| Repository | Files |
| --- | --- |
| Kinetra | `tools/cloud_setup.sh` (new), `.claude/settings.json` (new), [tools README](../../external/Kinetra/tools/README.md#cloud_setupsh), [development](../../external/Kinetra/docs/development.md#dependencies-and-options), the comment at the top of `.github/workflows/ci.yml` |
| Kinetra-Lab | `tools/cloud_setup.sh` (new), `.claude/settings.json` (new), [README](../../README.md#quick-start), comments in `.github/workflows/ci.yml`, [roadmap](../roadmap.md), this document and the [index](README.md), the `external/Kinetra` pointer |

No C++ source, CMake file or preset changes.

## Test plan

| Behavior | Proof | Level |
| --- | --- | --- |
| A fresh session gets everything | Remove the packages (`apt-get remove`), the uv tool and the submodule's checkout, then run `CLAUDE_CODE_REMOTE=true tools/cloud_setup.sh` in Lab: every package is installed, `clang-format --version` says 23.1.5, the submodule is checked out, one status line, exit 0 | Targeted |
| A second run does nothing | Run it again: under a second, no apt or uv call in the log | Targeted |
| A resumed session reuses clang-format | Run with a `PATH` that finds Ubuntu's clang-format first and a `CLAUDE_ENV_FILE`, twice: no uv install, the `PATH` line written once | Targeted |
| An old Kinetra commit | Check out a Kinetra commit without the script in the submodule and run Lab's script: one line that says so, exit 0 | Targeted |
| Outside a cloud session | Run it without `CLAUDE_CODE_REMOTE`: no output, nothing installed, exit 0 | Targeted |
| A moved submodule stays | Check out another commit in `external/Kinetra`, run the script: the submodule's `HEAD` is unchanged | Targeted |
| A failure is reported | Run Kinetra's script with a package name that does not exist: the status line says it failed and names the log, exit 0 | Targeted |
| The format check | CI's clang-format command on both repositories with the installed clang-format | Targeted |
| Build and tests | `cmake --preset dev` and the targeted build and test of `kinetra_lab_tests` in Lab; in Kinetra, `cmake --preset dev` and a targeted test | Targeted |
| The hooks themselves | `.claude/settings.json` parses (`python3 -m json.tool`); then a new cloud session on each pushed branch reports the hook's status line, `clang-format --version`, `git submodule status` and a `cmake --preset dev` that configures without installing anything | Full |
| Pull request CI | Lab's CI builds in full for these paths; Kinetra's CI as usual | Full |

## Review

The branch's own code review (the agent's `code-review`, on both repositories' diffs) found:

| Finding | Resolution |
| --- | --- |
| A resumed session whose hook `PATH` finds Ubuntu's clang-format first reinstalled the pinned one with `--force` each time and appended the `PATH` line again | Kinetra's script checks uv's copy first, installs only when it is missing or another version, and writes the line once (Kinetra 3b0fdf6) |
| An empty answer from `uv tool dir --bin` would have put the working directory on `PATH` | Reported as a failure instead (3b0fdf6) |
| The time limits (300 + 120 + 300 + 120 s) plus dpkg could exceed the hook's 900 s, and a killed hook prints nothing | The submodule fetch gets 180 s (Lab 408cb18); the limits add up to 720 s |
| A submodule whose checkout was interrupted got the message for an old Kinetra commit | The message names both causes and the command (408cb18) |
| `apt-get update` succeeded with a missing index, so the failure named the wrong step | `-o APT::Update::Error-Mode=any`, as in CI (3b0fdf6) |
| Lab's `ci.yml` stated the snapshot rule without the script's version, and its Ubuntu step did not name the script as a copy | Both comments name it (408cb18) |
| The tools README said `~/.local/bin` where the script uses uv's tool directory | Reworded (3b0fdf6) |
| The roadmap item says "with Kinetra" but Kinetra's roadmap has no item | Kept: the item closes with this pair of pull requests, so a Kinetra item would be added and removed in the same change |
| The pointer recorded a Kinetra commit not yet pushed | Kinetra's branch was pushed and its pull request opened before Lab's push |

Testing Lab's script on a clone found that a submodule at a Kinetra commit without the script made it fail without a status line; it now says so (a0a06b8).

## Verification

In this branch's cloud session (Ubuntu 24.04.5, *Default* environment):

| Check | Result |
| --- | --- |
| Lab's script on a fresh local clone, after `apt-get remove` of `libsdl2-dev`, `nlohmann-json3-dev`, `ccache`, `libgl-dev` and `libegl-dev` and `uv tool uninstall clang-format` | Loaded the submodule, installed the five packages and clang-format 23.1.5 in 6 s, one line, exit 0 |
| Second run | 0.11 s, no apt or uv call |
| Kinetra's script alone, fresh | Installed four packages and clang-format 23.1.5 in 9 s; nlohmann JSON left out |
| Without `CLAUDE_CODE_REMOTE` | No output, nothing installed, exit 0 (both scripts) |
| Submodule moved to another commit | Its `HEAD` stayed; at a Kinetra commit without the script, Lab's script said so and exited 0 |
| Failures | A package that does not exist, a submodule URL that does not answer, no `uv`, and a uv that names no directory each gave a `Cloud setup failed:` line and exit 0 |
| Resume with a `PATH` that finds Ubuntu's clang-format first | Twice: no uv install, the `CLAUDE_ENV_FILE` line written once, 0.2 s |
| CI's format command | Clean on Lab and on Kinetra (with its exclusions), with the installed clang-format 23.1.5 |
| Build and tests | Lab: a fresh `cmake --preset dev` found nlohmann JSON 3.11.3 and ccache; `kinetra_lab_tests` and `kinetra_i4sim_controller_tests` built, 52 of 52 passed. Kinetra: `cmake --preset dev`, `kinetra_runtime`, `kinetra_launcher_tests` and `kinetra_settings_view_tests` built, 38 of 38 non-graphics tests passed |
| `shellcheck`, `python3 -m json.tool`, `check_docs.py` in both repositories, `check_copies.py` | Clean; the copies check's hits are the package lists and clang-format names that stay valid, and Lab's `ci.yml` comments, which now name the script |

**The hooks themselves.** A new cloud session on each pushed branch, started from this session in the *Default* environment, reported without installing anything by hand:

| Session | Hook's line | Then |
| --- | --- | --- |
| Kinetra-Lab, `ccr-d6ebceac-13bzfb` | `Cloud setup: installed ccache libsdl2-dev libgl-dev libegl-dev libegl-mesa0 nlohmann-json3-dev; installed clang-format 23.1.5.` | Submodule at Kinetra 3b0fdf6; `feature`, `roadmap` and `verify` available; `cmake --preset dev` configured, the two suites built, 52 of 52 passed; format check rc 0. The hook held the first message for about 15 s |
| Kinetra, `build/cloud-session-setup` | `Cloud setup: installed ccache libsdl2-dev libgl-dev libegl-dev libegl-mesa0; installed clang-format 23.1.5.` | Configured, built `kinetra_runtime` and the two suites, 38 of 38 passed; format check rc 0. About 14 s |

The pull requests' CI is the Full level: Kinetra's builds and tests as usual, and Lab's builds in full for these paths.

## Pull request

`build(tools): set up a cloud session with a SessionStart hook`

### Summary

A cloud session of Kinetra-Lab started without SDL2, nlohmann JSON, ccache, CI's clang-format and the Kinetra submodule, so every code task installed them by hand first. A SessionStart hook in `.claude/settings.json` now runs `tools/cloud_setup.sh` at every start and resume of a cloud session: it loads the submodule when the checkout lacks it and runs Kinetra's `tools/cloud_setup.sh` with nlohmann JSON, which installs the missing Ubuntu packages and clang-format 23.1.5 from PyPI. A new session on this branch built and tested at once.

### Changes

- `tools/cloud_setup.sh` (new) and `.claude/settings.json` (new): the hook, on `startup` and `resume`, synchronous, 900 s limit; only when `CLAUDE_CODE_REMOTE=true`; always one status line and exit 0.
- `README.md`: a paragraph on what a cloud session sets up itself. `.github/workflows/ci.yml`: comments name the script as a copy of the package set and of the clang-format version.
- `external/Kinetra`: the pointer records Kinetra `build/cloud-session-setup`, which adds Kinetra's script, hook and docs.
- Design document, index, and the roadmap item *Ready-to-build cloud sessions*, removed as finished.

### Design

[Ready-to-build cloud sessions](2026-11-23-ready-to-build-cloud-sessions.md). The user chose a hook in the repository over the environment's setup script, and both repositories in this change.

### Verification

Fresh install, rerun, local, resume and failure paths of both scripts; CI's format check on both repositories with clang-format 23.1.5; Lab's `kinetra_lab_tests` and `kinetra_i4sim_controller_tests` (52 passed) and Kinetra's runtime and launcher suites (38 passed). A new cloud session on each branch reported the hook's line and configured, built, tested and checked formatting without installing anything by hand. Details in the design document's [Verification](2026-11-23-ready-to-build-cloud-sessions.md#verification). CI is the Full level.

### Review notes

The branch's code review found a reinstall on every resume, a possible empty `PATH` entry, time limits that could exceed the hook's, a misleading message for an interrupted submodule, and copies of the snapshot rule; all are fixed ([Review](2026-11-23-ready-to-build-cloud-sessions.md#review)). The hook holds a session's first message for about 15 s on a fresh VM.

### Dependencies and merge order

Kinetra pull request SMB-M87/Kinetra#47 merges first, with a merge commit; this pull request then merges with its pointer at the Kinetra branch commit it was verified with, and Kinetra's `lab-pointer` workflow moves the pointer to the merge commit afterwards. Both wait for the user: each changes a `ci.yml` comment.

### Follow-up

An environment setup script that calls `tools/cloud_setup.sh` would let Claude Code cache the result and cover sessions with several repositories and project threads; only the user can add it ([open points](2026-11-23-ready-to-build-cloud-sessions.md#open-points)).

## Open points

- An environment setup script that calls the same script would let Claude Code cache the result, and would cover sessions with several repositories and project threads, where repository hooks do not run. Only the user can add it, in the environment's settings.
