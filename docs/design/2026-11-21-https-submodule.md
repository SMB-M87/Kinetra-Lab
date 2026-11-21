# HTTPS submodule by default

Status: implemented · Roadmap: [phase 9, easier try-out](2026-11-18-quality-roadmap.md#phase-9-easier-try-out)

[Design index](README.md)

## Problem and design

`.gitmodules` pointed at `git@github.com:SMB-M87/Kinetra.git`. A visitor without an SSH key for GitHub saw `git clone --recurse-submodules` fail on the public engine repository and had to follow a workaround in the README. CI and the native Windows guide rewrote the URL with `insteadOf` for the same reason.

The submodule URL becomes `https://github.com/SMB-M87/Kinetra.git`. Kinetra is public, so fetching needs no credentials.

- The README quick start loses the SSH workaround. It keeps one line for old clones: `git submodule sync --recursive` copies the new URL into `.git/config`.
- CI drops both `insteadOf` lines; `docs/windows.md` drops its own.
- Developers who push engine branches over SSH set `url."git@github.com:".pushInsteadOf "https://github.com/"` once, as `external/README.md` now shows. Fetching stays HTTPS, pushing uses SSH.
- The decision row of the [tooling and CI design](2026-11-18-tooling-and-ci.md) stays as history.

## Test plan and review

- A fresh `git clone --recurse-submodules` of this branch over HTTPS, with no SSH configuration, checks out `external/Kinetra`.
- `python3 external/Kinetra/tools/check_docs.py` passes.
- The pull request's CI fetches the submodule without the rewrite (`checks` and `windows` jobs).

## Pull request

**build: fetch the Kinetra submodule over HTTPS**

The submodule URL is now HTTPS, so cloning with `--recurse-submodules` works without an SSH key. CI and the Windows guide no longer rewrite the URL; the README keeps one line for clones made before this change (`git submodule sync --recursive`), and `external/README.md` shows `pushInsteadOf` for developers who push over SSH. Also adds phase 9, easier try-out, to the quality roadmap.
