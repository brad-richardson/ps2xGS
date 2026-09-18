#!/bin/sh
# Step 7c: every synthetic capture must replay byte-exact on the CPU
# backend (gsreplay exit 0, which requires 0 max diff and VRAM exact
# on every present).
# Usage: replay_all.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
pass=0; fail=0
for f in "$DIR"/*.gscap; do
  if "$GSREPLAY" "$f" --backend cpu > /tmp/ps2xgs-replay-one.log 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    echo "FAIL: $f"
    cat /tmp/ps2xgs-replay-one.log
  fi
done
echo "identity-replay: pass=$pass fail=$fail"
exit $fail
