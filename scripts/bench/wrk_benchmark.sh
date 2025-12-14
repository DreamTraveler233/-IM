#!/usr/bin/env bash
set -euo pipefail

# Simple wrk benchmark harness for the IM server
# Usage: scripts/bench/wrk_benchmark.sh [base_url]
# Example: scripts/bench/wrk_benchmark.sh http://127.0.0.1:8080

BASE_URL=${1:-http://127.0.0.1:8080}
DO_AGGREGATE=${2:-false}
OPEN_REPORT=${3:-false}
OUT_DIR=bench_results/$(date +%Y%m%d_%H%M%S)
mkdir -p "$OUT_DIR"

ENDPOINTS=("/_/status" "/api/v1/common/send-test" "/media/test1m.bin")
THREADS=(2 4 8)
CONNS=(50 100 200)
DURATION=15s

echo "Benchmarking $BASE_URL -> results in $OUT_DIR"

for ep in "${ENDPOINTS[@]}"; do
  for t in "${THREADS[@]}"; do
    for c in "${CONNS[@]}"; do
      name=$(echo "$ep" | sed 's|/|_|g' | sed 's|__|_|g' | sed 's|:?||g' | sed 's/[^a-zA-Z0-9_]/_/g')
      out="$OUT_DIR/${name}_t${t}_c${c}.txt"
      echo "Running wrk $BASE_URL$ep (t=$t, c=$c) -> $out"
      wrk -t${t} -c${c} -d${DURATION} --latency "$BASE_URL$ep" > "$out" 2>&1 || true
    done
  done
done

echo "Done. Output directory: $OUT_DIR"
ls -la "$OUT_DIR"

# Optionally aggregate and open report
if [[ "$DO_AGGREGATE" == "report" || "$DO_AGGREGATE" == "aggregate" ]]; then
  # if virtualenv exists, activate it
  if [[ -d "$(dirname "$0")/venv" ]]; then
    # shellcheck disable=SC1090
    source "$(dirname "$0")/venv/bin/activate"
  fi
  python3 "$(dirname "$0")/aggregate_wrk.py" "$OUT_DIR"
  if [[ "$OPEN_REPORT" == "open" ]]; then
    bash "$(dirname "$0")/open_report.sh" "$OUT_DIR"
  fi
fi

exit 0
