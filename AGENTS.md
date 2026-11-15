# Kinetra-Lab Agent Guide

## Table of contents

- [Verification](#verification)
- [Documentation](#documentation)

[Project overview](README.md) · [Documentation index](docs/README.md)

## Verification

- After every change, including documentation and configuration changes, reconfigure and rebuild the project before reporting completion. Run the relevant tests after the build.
- Verify the normal root build against `external/Kinetra`: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DKINETRA_SOURCE_DIR="$PWD/external/Kinetra"`, `cmake --build build -j2`, and `ctest --test-dir build --output-on-failure`.
- Core-only tests, headless builds, cached binaries, temporary engine copies and alternate `KINETRA_SOURCE_DIR` checkouts do not replace this integration check. Verify the supplied checkout contains every public target/header required by Lab.
- After engine changes, also configure, build and test `external/Kinetra` independently. After runtime changes, enable graphics tests and smoke-test the launcher, Demo and Dotio with an actual graphics context.
- Report the commands, results and any environment blockers explicitly. A failed configure/build means verification is incomplete; never report the task as fully verified.

## Documentation

- Every repository content directory must always contain a `README.md`, including nested source, header, test, documentation and bundled dependency directories. Add missing guides when creating or changing directories; describe purpose, contents, responsibilities and relevant usage.
- Exclude only Git metadata, local tooling/account state, generated build outputs and locally downloaded dependency trees. Maintain checked-in dependency wrapper guides; a Git submodule is a separate repository whose own directory guides are maintained there, without copying or rewriting upstream documentation.
- Keep documentation navigable in both directions: each directory README links to its parent guide, immediate child guides and the documentation index; each documentation page links back to its directory guide and the project overview. Link related source, test and authoritative contract guides to one another, and keep these links synchronized when files move. Use concise overview/related-guide navigation like Artix rather than duplicating content.
- Keep README files and documentation concise and synchronized with implementation; link authoritative explanations instead of duplicating them. Test guides describe coverage and build/run commands.
- Place the table of contents below each maintained Markdown title/introduction, with the local heading links directly under `## Table of contents`. Immediately below that list, place compact parent, project overview, documentation index and child-guide links, following the layout in Artix/docs/networking.md. Do not add separate Navigation or On this page labels, or a navigation section at the end.
- Use `[TOC]` for local heading links only when the target Markdown renderer supports it. Otherwise maintain an explicit list of heading links so the guide remains navigable without a TOC extension. Verify relative links and heading anchors after documentation changes.
