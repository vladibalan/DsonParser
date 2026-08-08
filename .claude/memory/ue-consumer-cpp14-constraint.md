---
name: ue-consumer-cpp14-constraint
description: Parser is consumed by a UE 5.4.4 plugin via flat C ABI; stay UE-agnostic and build at default C++14
metadata: 
  node_type: memory
  type: project
  originSessionId: 986a3ea8-553e-4998-8077-f8e7d4488ab0
---

The DsonParser DLL is consumed by an **Unreal Engine 5.4.4 plugin** through its
flat `extern "C"` ABI (DsonParserAPI). Key constraints:

- **Stay UE-agnostic.** The parser must NOT include UE headers or use UE types.
  Only STL + RapidJSON internally; only C types cross the ABI boundary.
- **The C ABI decouples C++ standards.** Because only C types cross the boundary,
  the DLL's internal C++ standard is invisible to UE. UE 5.4 builds as C++20 by
  default, but that does NOT require the parser to match — no C++ ABI coupling.
- **Build at the toolset default = C++14.** The user explicitly chose (2026-06-04)
  to leave `.vcxproj` with NO `<LanguageStandard>` pin (v143 → C++14). Do not
  pin/bump to C++17 or C++20.

**How to apply:** When proposing refactors or "modern C++" polish, restrict to
C++14-available features. AVAILABLE in C++14: range-for, `std::accumulate`,
`using` aliases, trailing return types, declaration-in-`if`-condition
(`if (T* x = ...)`). OFF THE TABLE (C++17+): structured bindings, `std::string_view`,
`std::clamp`, `if (init; cond)` init-statements, inline variables, fold
expressions. Never add Unreal Engine dependencies to the parser. See
[[value-types-are-scaffolding]] and [[value-wrappers-in-ternary]].
