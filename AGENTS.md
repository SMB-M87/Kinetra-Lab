# Kinetra-Lab Agent Guide

## Table of contents

- [Documentation](#documentation)
- [Navigation](#navigation)

## Documentation

- Every repository content directory must always contain a `README.md`, including nested source, header, test, documentation and bundled dependency directories. Add missing guides when creating or changing directories; describe purpose, contents, responsibilities and relevant usage.
- Exclude only Git metadata, local tooling/account state, generated build outputs and locally downloaded dependency trees. Maintain checked-in dependency wrapper guides; a Git submodule is a separate repository whose own directory guides are maintained there, without copying or rewriting upstream documentation.
- Keep documentation navigable in both directions: each directory README links to its parent guide, immediate child guides and the documentation index; each documentation page links back to its directory guide and the project overview. Link related source, test and authoritative contract guides to one another, and keep these links synchronized when files move. Use concise overview/related-guide navigation like Artix rather than duplicating content.
- Keep README files and documentation concise and synchronized with implementation; link authoritative explanations instead of duplicating them. Test guides describe coverage and build/run commands.
- Maintain an explicit table of contents below the title/introduction of each Markdown guide and verify relative links and heading anchors after documentation changes.

## Navigation

[Project overview](README.md) · [Documentation index](docs/README.md)
