---
name: dson-zeroed-gzip-trailer
description: "Some DAZ packs ship gzip DSF with an all-zero CRC32+ISIZE trailer; triggers \"gzip CRC32 mismatch\"; tolerated parser-side since DsonParser 2.2.3 / DsonToUnreal 3.3.4."
metadata: 
  node_type: memory
  type: project
  originSessionId: fc3483a8-e971-4c94-b7c1-9d6e5e249bc9
---

DAZ content can ship gzip-wrapped DSF whose 8-byte gzip trailer (CRC32 + ISIZE) is written
all-zero while the DEFLATE payload itself is valid. Confirmed pack: 3D Universe "Pose Architect P1"
(G8F) under `data/DAZ 3D/Genesis 8/Female/Morphs/3D Universe/G8FPoseArchitectP1` — 52/53 `.dsf`
have the zeroed trailer (lone exception `...ButtocksOut.dsf`). Other G8F morph vendors sampled were
clean, so it is a per-product packaging defect, not a DAZ-wide convention. DAZ Studio loads these
because it does not validate the gzip trailer.

In-engine symptom: `LogDsonImporter: Warning: FDsonCatalog: LoadFromBuffer failed for '...': gzip
CRC32 mismatch`, and the asset is dropped (the shared loader `FDsonLoadedDocument::LoadFromFile`
resets the handle to null on parser failure). It is the catalog/enumeration path that hits these;
a plain-JSON `.duf` character (e.g. Cui 8.1) is unaffected unless its recipe references the pack.

Diagnose by reading the file's last 8 bytes: all-zero = this pattern (not corruption). Header is a
normal `1F 8B 08 00...`; only the footer is blank.

RESOLVED: DsonParser tolerates an all-zero trailer when the DEFLATE stream inflates cleanly (a clean
inflate already proves the payload whole) as of **2.2.3**; vendored into **DsonToUnreal 3.3.4**
(2026-06-21, commit b1c6c2a, tag v3.3.4) — build-verified, runtime confirmation is the user's editor
import. Genuine truncation and any present non-zero/mismatched trailer still fail.

**Why:** A real shipped, DAZ-loadable product was silently dropped because our check was stricter
than DAZ; "be liberal in what you accept" for a blank (not corrupt) footer.
**How to apply:** If "gzip CRC32 mismatch" recurs, first read the file tail — if it is NOT all-zero
it is real corruption; if it IS all-zero, suspect a stale/older vendored `DsonParser.dll` (pre-2.2.3)
rather than re-opening the parser fix. Parser change requests route as what/why through the user;
see [[parser-changes-route-through-user]] and [[dson-gzip-decompress-blocked]].
