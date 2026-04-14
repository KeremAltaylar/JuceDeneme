#!/usr/bin/env bash
set -euo pipefail

git add -A

if git diff --cached --quiet; then
  echo "No staged changes. Nothing to commit."
else
  git commit -m "Vibe build"
fi

git push

