#!/bin/sh
# G2: cpu identity on the 20 new captures (all exact).
# Usage: check_cpu_new.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
pass=0; fail=0
for b in tex1-filter-agree-nearest tex1-filter-agree-linear tex1-filter-disagree-lin tex1-filter-disagree-near tex1-filter-mixed-mmin mip-chain-mxl0 mip-chain-mxl1 mip-chain-mxl2 mip-chain-stq-mxl2 iso-dthe-off iso-dthe-on iso-colclamp-on iso-colclamp-off iso-scanmsk-zero iso-scanmsk-set iso-aa1-clear iso-aa1-set iso-zte-on-always iso-zte-off-always iso-zte-off-never; do
  if "$GSREPLAY" "$DIR/$b.gscap" --backend cpu > /tmp/ps2xgs-cpu-new-one.log 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    echo "FAIL: $b"
    cat /tmp/ps2xgs-cpu-new-one.log
  fi
done
echo "cpu-identity-on-new-captures: pass=$pass fail=$fail"
exit "$fail"
