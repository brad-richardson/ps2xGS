#!/bin/sh
# G3: every synthetic capture must replay byte-exact on the spike
# backend (gsreplay exit 0: 0 max diff and VRAM exact on every
# present). The spike backend is a cpu fork plus the CLUT-cache and
# RMW-lookup spikes, so any divergence is a spike bug.
# Usage: replay_spike.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
pass=0; fail=0
for f in "$DIR"/*.gscap; do
  if "$GSREPLAY" "$f" --backend spike > /tmp/ps2xgs-replay-spike-one.log 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    echo "FAIL: $f"
    cat /tmp/ps2xgs-replay-spike-one.log
  fi
done
echo "spike-identity: pass=$pass fail=$fail"
exit $fail
