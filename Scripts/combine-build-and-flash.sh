#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "$REPO_DIR"

echo "==> Pulling latest changes"
sudo -u gijs git pull --ff-only

"${SCRIPT_DIR}/build-only.sh"
"${SCRIPT_DIR}/flash-only.sh"

echo "==> Done"
