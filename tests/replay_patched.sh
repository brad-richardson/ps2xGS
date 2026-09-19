#!/bin/sh
# G5: every synthetic capture must replay byte-exact on the patched-cpu
# backend (gsreplay exit 0: 0 max diff and VRAM exact on every
# present). patched-cpu is the pristine upstream CPU backend plus the
# three upstream-patches/ files applied at build time, so any divergence
# is a patch bug.
# Usage: replay_patched.sh <synth-dir> <gsreplay-bin>
DIR="$1"
GSREPLAY="$2"
pass=0; fail=0
for f in "$DIR"/*.gscap; do
  if "$GSREPLAY" "$f" --backend patched-cpu > /tmp/ps2xgs-replay-patched-one.log 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    echo "FAIL: $f"
    cat /tmp/ps2xgs-replay-patched-one.log
  fi
done
echo "patched-identity: pass=$pass fail=$fail"
exit $fail
