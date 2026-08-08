---
name: ground-truth-before-fixing
description: "For a wrong runtime result (esp. composite/interpretation), get ground-truth data before writing a fix; don't ship hypothesis-fixes across user round-trips."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: bc344ea6-47fb-4dd3-b158-7b9b0a9402ed
---

On a wrong-looking runtime result, gather ground truth — the actual DAZ source (`map`/block fields), per-layer/diagnostic logging, or on-disk facts — and confirm the cause **before** writing a fix. Don't ship a reasoned-but-unconfirmed fix and hope.

**Why:** In the DsonArtisan Stage-2 LIE compositor (2026-06-11) the head bakes came out pure black. I shipped two reasoned-but-wrong fixes (source-alpha, then alpha-coverage) across two user editor round-trips before the user pasted the raw DAZ `map` JSON, which pinned the real cause in one step: `color:[0,0,0]` was being multiplied into the skin (a P2 *interpretation* bug on our side, not missing importer data). Each guess cost the user a build+import cycle, and the disk/log evidence (136 KB uniform output, `mean RGB=0.000`) was available the whole time.

**How to apply:** When a composite/interpretation looks wrong — read the actual DAZ source fields, add per-layer diagnostics, and check disk facts first; reach for the raw source early; only then write the fix. The Director can read editor runtime logs directly from `DsonArtisanHost/Saved/Logs/DsonArtisanHost.log` (see [[director-runtime-verify-via-user-import]]). Reinforces [[dson-formula-data-external-dsfs]] (verify DSON against the actual `.dsf`, not abstractly) and [[no-silent-fails]].

**Read the right *level* of ground truth (eyelash `M_DazDefault`, 2026-06-12):** the trap isn't only skipping source — it's stopping too shallow. I read the eyelash presets' surface *bindings* and concluded the base-load preset held the material (it bound the real leaf surfaces `Eyelashes Lower`/`Upper`), but its diffuse was a grey placeholder; the faithful look — `Genesis9_Eyelashes01_C.jpg` used as a `Cutout Opacity` transparency silhouette — lived in the stock MAT preset. A preset's surface *binding* is not its *content*: check channel values / texture refs, not just which surface it binds. Cost: a partly-wrong cross-repo bug report the Importer Director had to correct (fixed upstream in v1.6.1 + v1.6.2).
