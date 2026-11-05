#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

rm -rf .smoke
mkdir -p .smoke

set +e
timeout 30s nix develop --command bash -c '
  export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$(echo "$NIX_LDFLAGS" \
    | sed -E "s/-rpath [^ ]+//g; s/-L([^ ]+)/\1/g" \
    | tr -s " " ":" | sed "s/^://; s/:$//")"
  LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./out/bin/Telegram -workdir "$PWD/.smoke" -noupdate
'
status=$?
set -e

if [ "$status" -eq 124 ]; then
  echo "Smoke test: app stayed alive for 30s"
  exit 0
fi

exit "$status"
