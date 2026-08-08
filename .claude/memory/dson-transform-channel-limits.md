---
name: dson-transform-channel-limits
description: "measured DSON node transform-channel limit schema — all 3 transforms author min/max 100%, clamped is never false, scale is uniformly unbounded"
metadata: 
  node_type: memory
  type: reference
  originSessionId: d4a95460-d464-432c-8db4-d2b5b82184dd
---

Measured first-hand 2026-07-16 by walking `node_library` in
`D:\Daz_content\data\DAZ 3D\Genesis 9\Base\Genesis9.dsf` (plain JSON, 5.4 MB, 139
nodes = 138 `type=bone` + 1 `type=figure` root). Each node authors
`translation[]` / `rotation[]` / `scale[]` as arrays of channel objects carrying
`id`, `label`, `min`, `max`, and sometimes `clamped`.

**All three transforms author `min`/`max` on 100% of channels** — 417 channels
each, 1251 total, zero absences.

**`clamped` is authored FALSE exactly ZERO times in all 1251 channels.** 800 are
authored `true`; 451 are ABSENT. So the field carries information *only* through
presence — absent-vs-true, never true-vs-false. Per-transform absences:
rotation 3/417, translation 31/417, **scale 417/417 (never authored at all)**.
Consequence: any getter defaulting absent → `false` (the modifier-channel
precedent at `DsonTypes.cpp:928`) fabricates a value DAZ never writes, on 36% of
all channels.

**Scale carries zero information on the G9 base.** All 417 scale channels are
identical: `min=-10000, max=10000`, `clamped` absent, generic label. Even
`l_thigh` — a real bone with real translation limits — is `±10000`/ABSENT on
scale. It is a DAZ placeholder, not authored data.

**Translation carries real constraints** (the FR that prompted this measured only
rotation and missed it): `l_thigh` translation = `-10..10`, `clamped=true` on all
three axes; 6 channels locked `0..0`; most bounds finite.

**Labels are semantic only on rotation** — 8 distinct (`Bend`, `Twist`,
`Side-Side`, `Front-Back`, `Up-Down`, plus generic `X/Y/Z Rotate`). Translation
and scale author generic `X/Y/Z Translate` / `X/Y/Z Scale` only.

The `type=figure` root is a placement transform, not a joint: `±10000` with
`clamped` ABSENT on translation, rotation, and scale alike.

**Unmeasured:** follower/garment/hair/graft and third-party DSFs. Only the G9 base
has been read — do not generalize the scale-is-placeholder finding to content
that hasn't been walked.

Applies when scoping node transform-channel exposure; see
[[parser-faithful-no-cross-section-merge]] (R6.4 forbids the parser deciding
scale's uniformity makes it uninteresting — that is the consumer's call).
