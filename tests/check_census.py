#!/usr/bin/env python3
"""Step 7d: gscensus on the synthetic set must produce the expected
feature rows. Usage: check_census.py <census.json>"""
import json
import sys

path = sys.argv[1]
d = json.load(open(path))

failures = []


def check(cond, what):
    if not cond:
        failures.append(what)
        print("FAIL", what)


def has(m, *keys):
    return all(k in d.get(m, {}) for k in keys)


check(d.get("files") == 98, "files==98")
check(d.get("presents") == 98, "presents==98")
check(d.get("submits") == 138, "submits==138")

check(has("prim_type", "point", "line", "trilist", "tristrip", "trifan", "sprite"),
      "prim_type all 7")
check(has("prim_flags", "iip", "tme", "fge", "abe", "aa1", "fst", "fix", "ctxt", "pabe"),
      "prim_flags all")
check(has("tex_psm", "CT32", "CT24", "CT16", "T8", "T4", "T8H"), "tex_psm")
check(any("csm0" in k for k in d.get("clut", {})), "clut csm0")
check(any("csm1" in k for k in d.get("clut", {})), "clut csm1")
check(any("csa1" in k for k in d.get("clut", {})), "clut csa1")
check(has("clamp", "REPEAT/REPEAT", "CLAMP/CLAMP", "REGION_CLAMP/REGION_CLAMP",
          "REGION_REPEAT/REGION_REPEAT"), "clamp modes")
check(has("texa", "aem0", "aem1"), "texa aem")
check(has("tex1", "mmin0/mmag0/mxl0", "mmin1/mmag1/mxl0", "mmin0/mmag0/mxl1",
          "mmin0/mmag0/mxl2", "mmin1/mmag0/mxl0"), "tex1 mxl>0 + mixed")
check(len(d.get("alpha", {})) >= 6, "alpha combos>=6")
check(any("/C2/" in k for k in d.get("alpha", {})), "alpha C=FIX")
check(has("blend_misc", "pabe", "fba", "colclamp0", "dthe"), "blend misc")
check(has("atst", "NEVER", "ALWAYS", "LESS", "LEQUAL", "EQUAL", "GEQUAL", "GREATER",
          "NOTEQUAL", "OFF"), "atst all")
check(has("afail", "FB_ONLY", "ZB_ONLY", "RGB_ONLY"), "afail all")
check(any(k.startswith("date1") for k in d.get("date_datm", {})), "date1")
check(has("ztst", "NEVER/zte0", "ALWAYS", "GEQUAL", "GREATER"), "ztst all")
check(has("zbuf_psm", "Z32", "Z24", "Z16", "Z16S"), "zbuf psm")
check(d.get("zmsk", 0) > 0, "zmsk>0")
check(has("frame_psm", "CT32", "CT24", "CT16", "CT16S"), "frame psm")
check(d.get("fbmsk_nonzero", 0) > 0, "fbmsk>0")
check(len(d.get("scissor", {})) >= 2, "scissor extents>=2")
check(len(d.get("scanmsk", {})) >= 1, "scanmsk>=1")
check(d.get("fog_prims", 0) >= 3, "fog_prims>=3")
check(has("transfer_dir", "host->local", "local->host", "local->local"),
      "transfer dirs")
check(has("transfer_psm", "T8", "T4"), "transfer T8/T4")
check(d.get("consume_calls", 0) > 0, "consume>0")
check(d.get("readvram_calls", 0) > 0, "readvram>0")
check(d.get("writevram_calls", 0) > 0, "writevram>0")
check(any("en2" in k for k in d.get("pmode", {})), "pmode en2")
check(any("en1" in k and "en2" in k for k in d.get("pmode", {})), "pmode both")
check(has("smode2", "progressive", "field", "frame"), "smode2 all")

if failures:
    print(f"check_census: {len(failures)} failures")
    sys.exit(1)
print("check_census: all expectations hold")
