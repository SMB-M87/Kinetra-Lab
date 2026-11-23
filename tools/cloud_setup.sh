#!/usr/bin/env bash
# Prepares a Claude Code cloud session to build Kinetra-Lab: loads the Kinetra submodule when
# the checkout lacks it, then runs Kinetra's tools/cloud_setup.sh with Lab's own package,
# nlohmann JSON. .claude/settings.json runs it as a SessionStart hook. See
# external/Kinetra/tools/README.md#cloud_setupsh.
#
# Usage: tools/cloud_setup.sh
#   Does nothing outside a cloud session (CLAUDE_CODE_REMOTE other than "true"). Always exits
#   0 and prints one line, which Claude Code adds to the agent's context.
set -uo pipefail

[[ "${CLAUDE_CODE_REMOTE:-}" == "true" ]] || exit 0
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 0
log="${TMPDIR:-/tmp}/kinetra-cloud-setup.log"

# Only a checkout without the submodule: on a resumed session the agent may have moved it on
# purpose, and git submodule update would reset it to the commit Lab records.
if [[ ! -e external/Kinetra/.git ]]; then
    if ! timeout 300 git submodule update --init --recursive >> "$log" 2>&1; then
        echo "Cloud setup failed: git submodule update --init --recursive did not finish." \
            "Log: $log. Run it by hand, then tools/cloud_setup.sh."
        exit 0
    fi
fi
kinetra_setup=external/Kinetra/tools/cloud_setup.sh
if [[ ! -f "$kinetra_setup" ]]; then
    # A submodule moved to a Kinetra commit from before the script.
    echo "Cloud setup failed: external/Kinetra has no tools/cloud_setup.sh at its commit." \
        "Install the packages of the README's quick start by hand."
    exit 0
fi
# i4sim needs nlohmann JSON in every build, headless included (README, quick start).
exec bash "$kinetra_setup" nlohmann-json3-dev
