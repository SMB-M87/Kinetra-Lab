#!/usr/bin/env bash
# Decides whether the build of Lab can differ between two commits. ci.yml's "changes" job
# skips the build and tests of a pull request when it cannot, and the release workflow skips
# the cross build of the Windows zip. See tools/release/README.md.
#
# Usage: tools/release/needs_build.sh <base> <head>
#   Run from a Lab checkout of <head> with full history. Prints "false" only when every
#   changed Lab path is Markdown, docs/ or .agents/, except the three below, and, for a
#   moved external/Kinetra pointer, every path of that Kinetra range is Markdown, docs/,
#   .agents/, .claude/, .codex/config.toml, .github/ or tools/, except the two below.
#   Prints "true" otherwise and on any doubt: a missing or unrelated base, a failed
#   diff, or a Kinetra clone that does not answer. The reason goes to stderr.
#
# Neither the build, the tests nor the zip read these paths, with five exceptions that
# always build: kinetra_capture_tests reads README.md and checks docs/media/, in Lab and in
# Kinetra (the shared-UI GIFs), and the labs/i4sim/blueprints/ directory, its README
# included, is copied next to every executable and into the zip. Lab's .github/ is not among
# the paths because it is Lab's own CI; Kinetra's .claude/ and .codex/config.toml hold agent
# session settings, its .github/ is Kinetra's CI, and its tools/ hold helpers that no
# build step runs (CI's docs check, which runs on every pull
# request, uses check_docs.py). The zip's own README comes from
# tools/release/README-windows.txt. A new input under these paths comes out of the filter:
# a Lab path into read_anyway, a Kinetra path into kinetra_read_anyway, since read_anyway
# sees only Lab's own paths.
set -uo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <base> <head>" >&2
    exit 2
fi
base="$1"
head="$2"
# ^$ keeps the empty line of an empty diff out of the paths that are left.
docs='(\.md$|^docs/|^\.agents/|^$)'
read_anyway='(^README\.md$|^docs/media/|^labs/i4sim/blueprints/)'
kinetra_unread='(\.md$|^docs/|^\.agents/|^\.claude/|^\.codex/config\.toml$|^\.github/|^tools/|^$)'
kinetra_read_anyway='(^README\.md$|^docs/media/)'

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

# The decision fails closed: grep's status 1 means "no line", and any other failure, such as a
# broken pattern, builds. Here-strings instead of pipes, so no SIGPIPE can read as "no line".

# Sets $left to the lines of $2 that match none of the pattern $1.
left=""
keep_outside() {
    local status=0
    left="$(grep -v -E -e "$1" <<< "$2")" || status=$?
    [[ $status -le 1 ]] || build "grep failed (status $status) on $1"
}

# Succeeds when a line of $2 matches the pattern $1, fails when none does, and builds on an error.
has_match() {
    local status=0
    grep -q -E -e "$1" <<< "$2" || status=$?
    [[ $status -le 1 ]] || build "grep failed (status $status) on $1"
    return "$status"
}

keep_outside "^external/Kinetra\$|$docs" "$files"
[[ -z "$left" ]] || build "Lab files outside Markdown, docs/ and .agents/ changed"
if has_match "$read_anyway" "$files"; then
    build "README.md, docs/media/ or labs/i4sim/blueprints/ changed, which a test reads or the build copies"
fi

if has_match '^external/Kinetra$' "$files"; then
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
    if has_match "$kinetra_read_anyway" "$kfiles"; then
        build "Kinetra's README.md or docs/media/ changed, which kinetra_capture_tests reads"
    fi
    keep_outside "$kinetra_unread" "$kfiles"
    [[ -z "$left" ]] || build "Kinetra files outside the build-free paths changed: $left"
fi

echo "skip: all changed paths are build-free" >&2
echo false
