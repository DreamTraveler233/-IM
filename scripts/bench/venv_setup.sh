#!/usr/bin/env bash
set -euo pipefail

VENV_DIR="$(dirname "$0")/venv"
PYTHON=python3

if [[ ! -d "$VENV_DIR" ]]; then
  $PYTHON -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1090
source "$VENV_DIR/bin/activate"

pip install --upgrade pip
pip install matplotlib

echo "Virtualenv created at $VENV_DIR. Activate with: source $VENV_DIR/bin/activate" 
