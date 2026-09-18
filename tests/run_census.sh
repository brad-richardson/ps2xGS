#!/bin/sh
# Run gscensus over every capture in a directory.
# Usage: run_census.sh <gscensus-bin> <synth-dir> <json-out> <md-out>
GSCENSUS="$1"
DIR="$2"
JSON="$3"
MD="$4"
exec "$GSCENSUS" "$DIR"/*.gscap --json "$JSON" --md "$MD"
