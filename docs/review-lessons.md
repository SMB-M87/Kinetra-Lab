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

- **Record every result that *Verification* says follows later before the review starts, or list it under *Open points*** · added 2026-11-27 · source: [#147](https://github.com/SMB-M87/Kinetra-Lab/pull/147)
  #147 promised the tour's result in a later commit; the commits that fixed what the run found recorded no run of the fixed tour, which CI does not play. This carries Kinetra's #65 lesson from CI outcomes to local runs.

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

- **Merge Lab `main` before review when it changed captures or text that the branch changes too, and render those captures again on the merged head** · added 2026-11-27 · source: [#144](https://github.com/SMB-M87/Kinetra-Lab/pull/144)
  #143 and #142 refreshed the Demo and i4sim debug captures and added a showcase clip after #144 forked. Neither side of the binary conflicts was right (one showed the old menu, the other the old layers), and #144's green CI covered neither. This widens #78's lesson from the pointer to captures.

## Applications

- **When a pool reuses slots, end the slot's coordination state when the slot is refilled, keyed by the occupant's generation, not only when a step sees it empty** · added 2026-11-27 · source: [#139](https://github.com/SMB-M87/Kinetra-Lab/pull/139)
  A slot that a kill empties after the passing step and a spawn refills before the next one never looks empty, so the new enemy inherited the old plan.

- **Change the direction of a repeated-motion fixture before it settles into its pattern** · added 2026-11-27 · source: [#135](https://github.com/SMB-M87/Kinetra-Lab/pull/135)
  The stall detector held on to its first axis. A fixture that moved only along that axis passed while a mover that first drifted sideways could oscillate without a diagnosis.

- **Recheck how an existing coordinator restores a new temporary route** · added 2026-11-27 · source: [#140](https://github.com/SMB-M87/Kinetra-Lab/pull/140)
  Passing could restore an ordinary route for a mover with a checked parking detour. Waypoint pruning could also skip a checked corner. The remedy now rechecks shortcuts and current reservations before steering.

- **Turn a test scene that shows a new i4sim policy at work into a showcase scene in the same pull request** · added 2026-11-27 · source: [#140](https://github.com/SMB-M87/Kinetra-Lab/pull/140)
  #140's parked-blocker fixture showed MAKE WAY in its design capture, but the scene reached the SHOWCASE page and the README only in a follow-up, at the user's request. Add the scene file, its hand-built setup and its clip with the policy ([i4sim's how-to table](../labs/i4sim/README.md#changing-this-module)).

- **Test a temporary departure while its product is still in a producer queue** · added 2026-11-27 · source: [#140](https://github.com/SMB-M87/Kinetra-Lab/pull/140)
  Checking only the product's optional producer can miss a lost reservation. Verify the actual queue, including order, and exercise the product reaching the front while the mover is away.

- **When a measurement changes, search for its old figures as terms (`0.64 s`, `64%`), not only for the words that describe it** · added 2026-11-24 · source: #111
  Demo's README quoted the performance page's 0.64 s per second and blamed the grid rebuilds in its open points, which a search for the hotspot's wording did not find.

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

- **When a CI change copies a comment from Kinetra's `ci.yml` into Lab's, check what it claims against Lab's own jobs** · added 2026-11-23 · source: #98
  Lab has a `windows` job that Kinetra does not. #98 copied "and so does each install step" from Kinetra's header, true there, while Lab's `setup-msys2` step had no time limit.

## Media and tools

- **When a label moves, search for the label alone with `git grep -n -i` as well as the phrases given to `check_copies.py`, which reads only Markdown and YAML** · added 2026-11-27 · source: [#147](https://github.com/SMB-M87/Kinetra-Lab/pull/147)
  #147's phrases missed two code and CMake comments (`smoke.cmake`, i4sim's `main.cpp`) and three README sentences that name SCENARIOS in another word order.

- **When a change moves an action out of a menu, run every script that clicked it to its end, the tour included** · added 2026-11-27 · source: [#147](https://github.com/SMB-M87/Kinetra-Lab/pull/147)
  The tour opened i4sim's debug menu to click BLUEPRINTS and relied on it staying open for the next floor's MOTION rows. With BLUEPRINTS in the settings panel the menu stayed closed and the tour stopped there; CI runs only the clips, not the tour, so only a full local run showed it.

- **When a change flips a default that a script toggles, re-read every click on that row in every script, the clips and the tour, against the new start state** · added 2026-11-27 · source: [#144](https://github.com/SMB-M87/Kinetra-Lab/pull/144)
  A toggle click means the opposite once the default flips: #144's new layer defaults made the tour turn i4sim's HEATMAP on where it meant to hide it, and dropped Demo's ACTIVE AGENTS from its later chapters. Neither CI nor the clips play the tour.

- **Rebuild after changing an embedded image before capturing scenes that display it** · added 2026-11-24 · source: [#110](https://github.com/SMB-M87/Kinetra-Lab/pull/110)
  A combined capture can otherwise publish a fresh lab screenshot beside a launcher image rendered with the old still. Verify the combined command with a deliberately stale embedded image.

- **Compare a text file that a test reads by its content, never by its bytes** · added 2026-11-23 · source: #104
  A `std::ofstream` in text mode ends lines with `\r\n` on Windows, so #104's recorder test, which compared `framerate` with `15\n`, failed only in the `windows` job, as Demo's trace lines did before (41bdef0). Parse the number or the lines instead.

- **Test a UI identifier by using it as CLI input** · added 2026-11-22 · source: #74
  Display formatting and clipping can change an identifier even when the inspector and trace share a row builder. Pass the identifier shown to the user into the real executable.

- **Check seeded per-tick diagnostics across frame batching** · added 2026-11-22 · source: #76
  Frame-level spawn credit can move a spawn to an earlier simulated tick even when the inspector and CLI call the same row builder. Test the supported cadence and document whether observations are comparable across frame batching.
