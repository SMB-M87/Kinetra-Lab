# Author review in the feature cycle

Status: implemented · Branch: `docs/inline-feature-review`

The [shared workflow](../../external/Kinetra/docs/workflow.md) has the implementing session plan the affected scenarios, test critical regressions and review the complete change before pushing. The user can request an independent review.

## Lab adoption

Update the Lab guide, contributor README and pointer instructions to match the shared rules. The warning for a Lab PR that points to a Kinetra branch explains the existing merge order and full CI. Its condition and exit status are the same.

Merge the shared Kinetra workflow first. Lab adopts it through the regular pointer update after open changes to the pointer have merged.

## Verification

`python3 external/Kinetra/tools/check_docs.py` and `git diff --check` passed. The CI workflow matched its base with the warning and its comment removed. The shell step passed `bash -n`. Full verification runs on the pull request.
