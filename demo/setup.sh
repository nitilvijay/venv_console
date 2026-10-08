#!/usr/bin/env bash
# Creates a few sample projects with venvs for recording the demo GIF.
# Usage: demo/setup.sh [DIR]   (default: ~/venvscan-demo)
set -euo pipefail

DEMO_DIR="${1:-$HOME/venvscan-demo}"
PYTHON="${PYTHON:-python3}"

# project name -> packages to install
declare -A PROJECTS=(
    [web-api]="fastapi uvicorn requests"
    [data-analysis]="pandas matplotlib seaborn"
    [scraper]="requests beautifulsoup4 lxml"
    [ml-notebook]="numpy scikit-learn"
    [django-blog]="django pillow"
)

for project in "${!PROJECTS[@]}"; do
    venv="$DEMO_DIR/$project/.venv"
    if [[ -f "$venv/pyvenv.cfg" ]]; then
        echo "skip  $project (already exists)"
        continue
    fi
    echo "setup $project: ${PROJECTS[$project]}"
    "$PYTHON" -m venv "$venv"
    # shellcheck disable=SC2086  # package list is meant to word-split
    "$venv/bin/pip" install --quiet --disable-pip-version-check ${PROJECTS[$project]}
done

echo "Done. Try: venvscan $DEMO_DIR"
