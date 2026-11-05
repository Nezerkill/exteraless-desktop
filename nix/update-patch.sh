#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

base_file="nix/upstream-base"
if [ ! -f "$base_file" ]; then
  echo "Missing $base_file with the pinned upstream commit." >&2
  exit 1
fi
line="$(sed -n '1p' "$base_file")"
base="$(echo "$line" | awk '{print $1}')"
expected_tree="$(echo "$line" | awk '{print $2}')"
upstream="$(echo "$line" | awk '{print $3}')"
if [ -z "$base" ] || ! git cat-file -e "$base^{commit}" 2>/dev/null; then
  echo "Pinned upstream base '$base' is not a commit in this repository." >&2
  exit 1
fi
tree="$(git rev-parse "$base^{tree}")"
if [ -n "$expected_tree" ] && [ "$tree" != "$expected_tree" ]; then
  echo "Base tree $tree does not match the pinned tree $expected_tree." >&2
  exit 1
fi
echo "Base $base (tree $tree, upstream ${upstream:-unknown})"

tag="$(git rev-parse -q --verify 'v7.3.1^{commit}' || true)"
if [ "$tag" != "$base" ]; then
  echo "Warning: local tag v7.3.1 (${tag:-missing}) differs from the" >&2
  echo "pinned base $base; the patch is generated against the pinned base." >&2
fi

git add -N exteraless Telegram
git diff "$base" --binary -- CMakeLists.txt Telegram exteraless \
  > nix/patches/exteraless.patch

echo "Updated nix/patches/exteraless.patch against $base"

if [ "${1:-}" = "--check" ]; then
  tmp="$(mktemp -d "${TMPDIR:-/tmp}/exteraless-patch-check.XXXXXX")"
  cleanup() {
    git worktree remove --force "$tmp" >/dev/null 2>&1 || true
    rm -rf "$tmp"
  }
  trap cleanup EXIT
  git worktree add --detach "$tmp" "$base" >/dev/null
  git -C "$tmp" apply --check "$PWD/nix/patches/exteraless.patch"
  git -C "$tmp" apply "$PWD/nix/patches/exteraless.patch"
  sha256sum "$PWD/nix/patches/exteraless.patch"
  echo "Patch applies cleanly to $base"
fi
