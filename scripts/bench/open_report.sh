#!/usr/bin/env bash
set -euo pipefail

REPORT_DIR=${1:-bench_results}
PORT=${2:-}
START_PORT_RANGE=${3:-8082}
if [[ -f "$REPORT_DIR/report.html" ]]; then
  REPORT_PATH="$REPORT_DIR/report.html"
  REPORT_DIR_BASE="$REPORT_DIR"
else
  if [[ -d "$REPORT_DIR" ]]; then
    DIR=$(ls -1 "$REPORT_DIR" | tail -n 1)
    REPORT_DIR_BASE="$REPORT_DIR/$DIR"
    REPORT_PATH="$REPORT_DIR_BASE/report.html"
  else
    echo "report dir not found: $REPORT_DIR"
    exit 1
  fi
fi
if [[ ! -f "$REPORT_PATH" ]]; then
  echo "report not found: $REPORT_PATH"
  exit 1
fi
# Serve the report folder
cd "$REPORT_DIR_BASE"
if [[ -n "$PORT" ]]; then
  # use specified port if free
  if ss -ltn | grep -qE ":$PORT\b"; then
    echo "端口 $PORT 已被占用，请选择其他端口或不指定端口以自动选择。"
    exit 1
  fi
  python3 -m http.server "$PORT" &
  PID=$!
  echo "正在提供报告: http://127.0.0.1:$PORT/report.html (pid=$PID)"
else
  # find free port starting from START_PORT_RANGE
  found_port=""
  for p in $(seq $START_PORT_RANGE $((START_PORT_RANGE+100))); do
    if ! ss -ltn | grep -qE ":$p\b"; then
      found_port=$p
      break
    fi
  done
  if [[ -z "$found_port" ]]; then
    echo "未找到空闲端口（尝试了 $START_PORT_RANGE..$((START_PORT_RANGE+100))），请手动指定。"
    exit 1
  fi
  python3 -m http.server "$found_port" &
  PID=$!
  echo "正在提供报告: http://127.0.0.1:$found_port/report.html (pid=$PID)"
fi

echo "按 Ctrl-C 停止服务"
wait $PID
