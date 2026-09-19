#!/bin/sh
# G2: strict must DIFF (gsreplay exit 1) on mip captures (sensitivity).
# Usage: check_strict_diffs.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
fail=0
for b in mip-chain-mxl1 mip-chain-mxl2 mip-chain-stq-mxl2; do
  if "$GSREPLAY" "$DIR/$b.gscap" --backend strict > /tmp/ps2xgs-strict-diff-one.log 2>&1; then
    echo "EXPECTED-DIFF-BUT-EXACT: $b"
    fail=1
  else
    code=$?
    if [ "$code" -ne 1 ]; then
      echo "UNEXPECTED-EXIT $code (expected 1): $b"
      cat /tmp/ps2xgs-strict-diff-one.log
      fail=1
    fi
  fi
done
if [ "$fail" -eq 0 ]; then
  echo "strict-diffs-on-mip: all 3 diff as expected"
else
  echo "strict-diffs-on-mip: FAILED"
fi
exit "$fail"
