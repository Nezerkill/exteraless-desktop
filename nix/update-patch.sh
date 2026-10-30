#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

git add -N exteraless \
	Telegram/SourceFiles/settings/sections/settings_exteraless.cpp \
	Telegram/SourceFiles/settings/sections/settings_exteraless.h
git diff v7.3.1 --binary -- CMakeLists.txt Telegram exteraless > nix/patches/exteraless.patch

echo "Updated nix/patches/exteraless.patch"
