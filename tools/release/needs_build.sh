#!/usr/bin/env bash
# Decides whether the Windows zip can differ between two Lab commits, so the release
# workflow skips the cross build when it cannot. See tools/release/README.md.
#
# Usage: tools/release/needs_build.sh <base> <head>
#   Run from a Lab checkout of <head> with full history. Prints "false" only when every
#   changed path is Markdown, docs/ or .agents/, in Lab and, for a moved external/Kinetra
#   pointer, in that Kinetra range too. Prints "true" otherwise, and on any doubt: a missing
#   or unrelated base, a failed diff, or a Kinetra clone that does not answer. The reason
#   goes to stderr.
#
# The filter is the one in ci.yml's "changes" job; change both together. Nothing in the zip
# is read from these paths: the zip's own README comes from tools/release/README-windows.txt.
set -uo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <base> <head>" >&2
    exit 2
fi
base="$1"
head="$2"
docs='(\.md$|^docs/|^\.agents/)'

build() {
    echo "build: $1" >&2
    echo true
    exit 0
}

git cat-file -e "$base^{commit}" 2> /dev/null || build "base $base is not a commit here"
git cat-file -e "$head^{commit}" 2> /dev/null || build "head $head is not a commit here"
git merge-base --is-ancestor "$base" "$head" 2> /dev/null || build "$base is not an ancestor of $head"

# --no-renames: a rename lists its old and its new path, so a source moved into docs/ or
# renamed to .md still counts as a source change.
files="$(git diff --no-renames --name-only "$base" "$head")" || build "git diff $base $head failed"
printf 'Lab %s..%s changed:\n%s\n' "$base" "$head" "${files:-(nothing)}" >&2

sources="$(printf '%s\n' "$files" | grep -v -x 'external/Kinetra' | grep -v -E "$docs" | grep . || true)"
[[ -z "$sources" ]] || build "Lab files outside Markdown, docs/ and .agents/ changed"

if printf '%s\n' "$files" | grep -q -x 'external/Kinetra'; then
    old="$(git rev-parse "$base:external/Kinetra")" || build "no Kinetra pointer at $base"
    new="$(git rev-parse "$head:external/Kinetra")" || build "no Kinetra pointer at $head"
    url="$(git show "$head:.gitmodules" | git config -f - submodule.external/Kinetra.url)" \
        || build "no Kinetra URL in .gitmodules at $head"
    kinetra="$(mktemp -d)"
    trap 'rm -rf "$kinetra"' EXIT
    git clone -q --filter=blob:none --no-checkout "$url" "$kinetra" || build "could not clone Kinetra from $url"
    kfiles="$(git -C "$kinetra" diff --no-renames --name-only "$old" "$new")" \
        || build "could not diff Kinetra $old..$new"
    printf 'Kinetra %s..%s changed:\n%s\n' "$old" "$new" "${kfiles:-(nothing)}" >&2
    ksources="$(printf '%s\n' "$kfiles" | grep -v -E "$docs" | grep . || true)"
    [[ -z "$ksources" ]] || build "Kinetra files outside Markdown, docs/ and .agents/ changed"
fi

echo "skip: only Markdown, docs/ or .agents/ changed" >&2
echo false
