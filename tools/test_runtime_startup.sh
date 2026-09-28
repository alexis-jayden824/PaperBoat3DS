#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python_command=${PYTHON:-python3}

cd "$project_root"
"$python_command" tests/test_runtime_startup.py
