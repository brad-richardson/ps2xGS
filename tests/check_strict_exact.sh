#!/bin/sh
# G2: strict must stay EXACT (gsreplay exit 0) on controls (specificity).
# Usage: check_strict_exact.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
pass=0; fail=0
for b in tex1-filter-agree-nearest tex1-filter-agree-linear mip-chain-mxl0 iso-dthe-off iso-colclamp-on iso-scanmsk-zero iso-aa1-clear iso-zte-on-always iso-zte-off-always; do
  if "$GSREPLAY" "$DIR/$b.gscap" --backend strict > /tmp/ps2xgs-strict-exact-one.log 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    echo "FAIL (expected exact): $b"
    cat /tmp/ps2xgs-strict-exact-one.log
  fi
done
echo "strict-exact-on-control: pass=$pass fail=$fail"
exit "$fail"
