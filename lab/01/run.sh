#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

make all

mkdir -p build/output report

RESULTS="report/result.csv"
echo "benchmark,nodes,nets,cut,best_cut,gap_percent,runtime_seconds" > "$RESULTS"

best_cut_for() {
  case "$1" in
    ibm01) echo 200 ;;
    ibm02) echo 307 ;;
    ibm03) echo 951 ;;
    ibm04) echo 573 ;;
    ibm05) echo 1706 ;;
    ibm06) echo 962 ;;
    ibm07) echo 878 ;;
    ibm08) echo 1140 ;;
    ibm09) echo 620 ;;
    ibm10) echo 1253 ;;
    ibm11) echo 1051 ;;
    ibm12) echo 1919 ;;
    ibm13) echo 831 ;;
    ibm14) echo 1842 ;;
    ibm15) echo 2730 ;;
    ibm16) echo 1827 ;;
    ibm17) echo 2270 ;;
    ibm18) echo 1521 ;;
    *) echo "" ;;
  esac
}

found=0
for case_path in data/*.hgr; do
  if [[ ! -e "$case_path" ]]; then
    break
  fi

  found=1
  echo "Running $case_path"
  start_time=$(date +%s)
  run_output=$(./build/main "$case_path")
  end_time=$(date +%s)
  runtime=$((end_time - start_time))

  echo "$run_output" | tee "build/output/$(basename "$case_path" .hgr).log"

  nodes=$(echo "$run_output" | awk '/Num nodes:/ {print $3}')
  nets=$(echo "$run_output" | awk '/Num nets:/ {print $3}')
  cut=$(echo "$run_output" | awk '/Cut:/ {print $2}')
  benchmark=$(basename "$case_path" .hgr)
  best_cut=$(best_cut_for "$benchmark")
  if [[ -n "$best_cut" && "$best_cut" -gt 0 ]]; then
    gap_percent=$(awk -v cut="$cut" -v best="$best_cut" 'BEGIN { printf "%.2f", (cut - best) * 100.0 / best }')
  else
    gap_percent=""
  fi
  echo "$benchmark,$nodes,$nets,$cut,$best_cut,$gap_percent,$runtime" >> "$RESULTS"
done

if [[ "$found" -eq 0 ]]; then
  echo "No data/*.hgr files found. Put ISPD benchmarks under lab/01/data/."
fi
