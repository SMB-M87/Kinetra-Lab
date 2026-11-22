# Review lessons

Mistakes that reviews and CI found in Kinetra-Lab work and that can happen again, each with the reason and what to do instead. Agents read this page and [Kinetra's review lessons](../external/Kinetra/docs/review-lessons.md) before they build a change (`feature` skill) and before they review one (`pr-review` skill). Lessons that apply to both repositories live on Kinetra's page; this page holds what is specific to Lab: the applications, the submodule pointer, media and the Lab build.

## Table of contents

- [Format](#format)
- [Submodule and merge order](#submodule-and-merge-order)
- [Applications](#applications)
- [Media and tools](#media-and-tools)

[Documentation index](README.md) · [Agent rules](../AGENTS.md) · [Roadmap](roadmap.md)

## Format

As on [Kinetra's page](../external/Kinetra/docs/review-lessons.md#format): one list item per lesson, under its topic, with the date and the pull request it came from. The author adds a lesson in a commit on the pull request where it was found; a lesson that keeps coming back is proposed to the user as a rule.

## Submodule and merge order

- **Mark a pull request "do not merge yet" while it still needs a commit after another merges** · added 2026-11-22 · source: #65
  #65 was to move its pointer to the Kinetra merge commit after Kinetra #21 merged, but it was merged in that same minute, before the pointer commit arrived, and #66 had to follow. State it in the description, or make the pull request a draft until the last commit is pushed, and say when it is ready.

## Applications

No lessons yet.

## Media and tools

No lessons yet.
