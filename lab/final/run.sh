#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

PARTS="${PARTS:-4}"
R="${R:-0.0}"
PASSES="${PASSES:-8}"

make all
mkdir -p build/output report

RESULTS="report/result.csv"
echo "benchmark,nodes,nets,parts,min_r,cut,imbalance,runtime_seconds" > "$RESULTS"

found=0
for case_path in data/*.hgr; do
  if [[ ! -e "$case_path" ]]; then
    break
  fi

  found=1
  benchmark="$(basename "$case_path" .hgr)"
  echo "Running $case_path with PARTS=$PARTS R=$R PASSES=$PASSES"

  run_output="$(./build/main "$case_path" --parts "$PARTS" --min-r "$R" --passes "$PASSES")"
  echo "$run_output" | tee "build/output/${benchmark}_k${PARTS}.log"

  nodes="$(echo "$run_output" | awk '/Num nodes:/ {print $3}')"
  nets="$(echo "$run_output" | awk '/Num nets:/ {print $3}')"
  cut="$(echo "$run_output" | awk '/Cut:/ {print $2}')"
  imbalance="$(echo "$run_output" | awk '/Imbalance:/ {print $2}')"
  runtime="$(echo "$run_output" | awk '/Runtime seconds:/ {print $3}')"

  echo "$benchmark,$nodes,$nets,$PARTS,$R,$cut,$imbalance,$runtime" >> "$RESULTS"
done

if [[ "$found" -eq 0 ]]; then
  echo "No data/*.hgr files found. Put ISPD benchmark .hgr files under lab/final/data/."
fi
