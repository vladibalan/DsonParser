---
name: dsonparser-consumer-dynamic-load-only
description: "UE plugin consumes DsonParser.dll via dynamic load only (GetDllHandle/GetProcAddress); the .lib import-link path is unused, so .lib-linked DsonTest2 cannot exercise the real load model"
metadata: 
  node_type: memory
  type: project
  originSessionId: 53235349-66ca-4cab-9d0e-d039178f5776
---

The UE 5.4.4 plugin integrates DsonParser **by dynamic load only** — `FPlatformProcess::GetDllHandle` (LoadLibrary) + `GetDllExport` (GetProcAddress) on `DsonParser.dll`. The generated `DsonParser.lib` import library / load-time-link path is **not used** by the consumer (confirmed 2026-06-12).

**Why:** it changes how in-repo verification must be judged. The `DsonTest2` harness **load-time-links `DsonParser.lib`**, so the DLL is mapped at process start and no thread can predate its load. DsonTest2 is therefore structurally unable to reproduce any load-model-sensitive behavior the real consumer hits — most acutely the function-local `thread_local` / first-use-init TLS case behind the 1.6.0 per-thread last-error fix (the original failure was a file-scope `thread_local` not initialized for a thread predating a `GetDllHandle`-loaded DLL). See [[director-runtime-verify-via-user-import]], [[no-silent-fails]].

**How to apply:** to verify load-model-sensitive behavior in-repo (rather than only in the UE host), use a **separate target that does NOT link the .lib**: spawn a worker thread -> `LoadLibrary` the built DLL -> `GetProcAddress` -> call from the pre-existing thread -> assert. The .lib-linked DsonTest2 proves logic/semantics only, never the dynamic-load mapping. Relates to [[ue-consumer-cpp14-constraint]], [[foreign-host-dup-importer-instance]].
