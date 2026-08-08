---
name: value-wrappers-in-ternary
description: "Dson value wrappers (Int/Float/String/etc.) need explicit cast in a ?: against a primitive"
metadata: 
  node_type: memory
  type: project
  originSessionId: 986a3ea8-553e-4998-8077-f8e7d4488ab0
---

The `Dson::` value wrappers in `DsonParser/DsonDataTypes.h` (`Int`, `Float`,
`String`, `Bool`, `Url`) each have BOTH an implicit conversion operator
(`operator int()`, etc.) AND a converting constructor from the primitive. That
makes them ambiguous as a branch of a ternary against a primitive literal:

```cpp
return geom ? geom->vertex_count : 0;   // C2445: 'const Dson::Int' vs 'int' ambiguous
```

A plain `return geom->vertex_count;` is fine (the conversion operator applies to
the `int` return type), but in `?:` the compiler can convert each arm to the
other and can't pick a common type.

**Fix:** force the wrapper arm to the primitive explicitly:
```cpp
return geom ? static_cast<int>(geom->vertex_count) : 0;
```

**Why:** Hit during the Doc()/At() accessor refactor (2026-06-04) — 5×
DsonParserAPI.cpp accessors returning `Int` fields (`vertex_count`,
`polygon_count`, `skin.vertex_count`) failed with C2445.
**How to apply:** When converting accessors that return a `Dson::` wrapper field
to a `cond ? wrapper : sentinel` form, wrap the field in `static_cast<int>` /
`static_cast<double>`. Fields that are already raw `int`/`double` (e.g.
`UVSet::vertex_count`, `Node::general_scale`) don't need it. See
[[value-types-are-scaffolding]].
