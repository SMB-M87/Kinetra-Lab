# Review lessons

Mistakes that reviews and CI found in Kinetra-Lab work and that can happen again, each with the reason and what to do instead. Agents read this page and [Kinetra's review lessons](../external/Kinetra/docs/review-lessons.md) before they build a change (`feature` skill) and before they review one (`pr-review` skill). Lessons that apply to both repositories live on Kinetra's page; this page holds what is specific to Lab: the applications, the submodule pointer, media and the Lab build.

## Table of contents

- [Format](#format)
- [Roadmap and decisions](#roadmap-and-decisions)
- [Submodule and merge order](#submodule-and-merge-order)
- [Applications](#applications)
- [CI](#ci)
- [Media and tools](#media-and-tools)

[Documentation index](README.md) · [Agent rules](../AGENTS.md) · [Roadmap](roadmap.md)

## Format

As on [Kinetra's page](../external/Kinetra/docs/review-lessons.md#format): one list item per lesson, under its topic, with the date and the pull request it came from. The author adds a lesson in a commit on the pull request where it was found; a lesson that keeps coming back is proposed to the user as a rule.

## Roadmap and decisions

- **When a decision session closes a roadmap item, carry every question it listed into a decision or a remaining item** · added 2026-11-22 · source: #67
  The removed item *Decide on making way and a temporary motion fallback* listed three questions; the session answered two, and the third (the departure check) dropped out of the plan until the agent review found it. Before removing such an item, tick off each of its questions against the Decisions table.

- **Take an item out of *Now* when its branch merges, even when the branch was one step of a larger goal** · added 2026-11-23 · source: #83
  *Now* holds only items with a branch or an open pull request. #83 and Kinetra #37 left *Leaner agentic cycle* in *Now* with no branch, while its next step was already an item in *Next*; give the remaining step its own item, or move the item back to *Next*.

- **Measure Git history from GitHub or a full clone, never from a cloud session's checkout** · added 2026-11-23 · source: #83
  A cloud session clones the last 50 commits; #83's measurement took the 57 merges it saw for the whole period and called the 24 older ones stacked.

## Submodule and merge order

- **Move the pointer in a pointer-only pull request of its own, never as the last commit of a reviewed one** · added 2026-11-22 · source: #65, #67, #68
  #65 was to move its pointer to the Kinetra merge commit after Kinetra #21 merged, but it was merged in that same minute and #66 had to follow; #67 merged before Kinetra #26 and #68 followed. A late pointer commit also runs a reviewed pull request's CI again. Since 2026-11-22 the rule is a separate pointer-only pull request, merged by the agent; a pull request that must not merge yet for another reason is still a draft until it is ready.

- **When Lab `main` moves its Kinetra pointer while a Lab pull request records its own, merge Kinetra `main` into the Kinetra branch, follow it, and merge Lab `main` before review** · added 2026-11-23 · source: #78
  The inspector stack merged after #78's CI ran. #78's pointer then lacked Kinetra #31, its green CI covered a `main` that no longer existed, and either side of the pointer conflict broke the build.

- **Carry a review fix to every copy of the same job, page or claim in the other repository of the stack, in both directions** · added 2026-11-23 · source: #78, #83
  Kinetra #33's Ubuntu job got the CMake path fix and a lesson; Lab's copy of the job, three minutes older, kept the runner's CMake until its own review. In #83 a Lab fix left two Kinetra lines that described the old behavior.

- **After review fixes, update every section of the design and the description that names the changed step, not only the review table and the decision row** · added 2026-11-23 · source: #78
  Round 1's fixes on #78 reached the *Ubuntu job* decision and the review table, but not the *Design* bullet, *Affected areas* or the description, which still named the old command. Kinetra #33's step 2 needed the same correction.

## Applications

No lessons yet.

## CI

- **A path filter that lets CI skip work must list both sides of a rename** · added 2026-11-22 · source: #69
  `git diff --name-only` detects renames by default and prints only the new path, so a source moved into `docs/` passed a docs-only filter. Use `--no-renames`, and test a rename case.

- **A check that runs only on a skip path must still fail a required status** · added 2026-11-22 · source: #69
  The docs check of a skipped pull request ran in a job that is not required, so its failure would have left the required, skipped job counting as passed. Let the required job run when the deciding job fails.

- **Make a smoke run fail when the program falls back instead of doing what its arguments ask** · added 2026-11-22 · source: #73
  `kinetra_smoke:i4sim-run` passed on exit code 0 whether or not its blueprint loaded, because i4sim falls back to the blueprint screen. Check the outcome the run is named for, with a failure pattern or an exit code.

- **When a rule changes which files a pull request carries, recheck every CI decision that sorts pull requests by their file list** · added 2026-11-23 · source: #83
  One design document per change lets a Lab pull request that follows a Kinetra branch hold only the pointer, and the pointer-only check then failed it until Kinetra merged.

- **When CI runs a trusted copy of a script, take it from the base branch's tip, not from the merge base** · added 2026-11-23 · source: #83
  A branch keeps its merge base until it merges `main`, so it would decide with the filter it forked from; right after #83, a README-only pull request from an older branch would have skipped `kinetra_capture_tests`.

- **When a change says where a CI step runs, search for the step's command and step name as well, not only the words the pages use for it** · added 2026-11-23 · source: #87
  The copies search for "docs check" with "only" found Lab's `ci.yml` header but not `README.md`, which names the step by its command, `check_docs.py`.

## Media and tools

- **Test a UI identifier by using it as CLI input** · added 2026-11-22 · source: #74
  Display formatting and clipping can change an identifier even when the inspector and trace share a row builder. Pass the identifier shown to the user into the real executable.

- **Check seeded per-tick diagnostics across frame batching** · added 2026-11-22 · source: #76
  Frame-level spawn credit can move a spawn to an earlier simulated tick even when the inspector and CLI call the same row builder. Test the supported cadence and document whether observations are comparable across frame batching.
