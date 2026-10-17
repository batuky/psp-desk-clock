#!/bin/sh
# Run a command inside the dev container with the repo mounted at /src.
#   tools/dev.sh make test
#   tools/dev.sh make core-psp-check
# Builds the image on first use. Works from Git Bash and WSL.
set -e
IMAGE=psp-desk-clock-dev
ROOT=$(cd "$(dirname "$0")/.." && (pwd -W 2>/dev/null || pwd))
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  MSYS_NO_PATHCONV=1 docker build -t "$IMAGE" "$ROOT/tools"
fi
MSYS_NO_PATHCONV=1 docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" "$@"
