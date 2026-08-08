---
name: dson-gzip-decompress-blocked
description: "Bitdefender blocks PowerShell/Bash in-memory gzip-decompress of DAZ .dsf/.duf (surfaces as a classifier \"temporarily unavailable\" error); ask the user to decompress + upload plain JSON — but check magic bytes first, some DAZ content (e.g. base correctives) is plain JSON readable directly"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 86d8528c-063f-41cb-9f5e-ce4793bc3050
---

DAZ `.dsf`/`.duf` assets are frequently gzip-compressed binary (that's why DsonParser
ships its own inflater). Inspecting one statically as Director needs decompression, but
running an in-memory gzip decompress through the PowerShell/Bash tools here is blocked by
the user's Bitdefender — the heuristic flags PowerShell reading a binary file +
`System.IO.Compression.GZipStream` (or equivalent) as suspicious. It surfaces as
"claude-opus-4-8[1m] is temporarily unavailable, so auto mode cannot determine the safety
of PowerShell/Bash" — looks like a model/classifier outage, but it's the AV killing the
command. Confirmed by the user 2026-06-15.

**Why:** I treated it as a transient infra outage and burned ~5 retries (PowerShell + Bash)
before the user explained the cause; the shell path is a dead end for this, not a flaky one.

**How to apply:** when you need a compressed DSON's JSON, don't retry the shell — ask the
user to decompress and upload the plain `.json` (they can run it locally; the AV only blocks
the agent's shell). Then Grep/Read the uploaded text. The parser inflates transparently at
load, so this only affects static Director inspection, never the importer code. Read-only
tools (Glob, Read, Grep) still work and are not classifier-gated.

**But not all DAZ content is gzipped — check before asking.** Under `D:/Daz_content` the
store is mixed: most character/JCM morph `.dsf` are gzip (user must decompress + upload),
but some — notably base correctives under `Morphs/DAZ 3D/Base Correctives/` — are stored as
plain JSON and Read directly. Peek the first bytes first (`1F 8B` = gzip → upload needed;
`7B` / `{` = plain → just Read it); this avoids needless round-trips (2026-06-15: read
`body_cbs_*.dsf` directly to close a transitive-classifier check the user would otherwise
have had to upload). See [[ask-for-files-before-diagnostics]], [[daz-content-library-root]].
