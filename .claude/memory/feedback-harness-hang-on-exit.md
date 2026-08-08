---
name: feedback-harness-hang-on-exit
description: "DsonTest2 harness blocks on stdin at \"Press Enter to exit...\" when file-load path triggers; piped EOF exits with code 255 not 0"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: fd464e6a-2a26-408a-b98c-47eaa48395b6
---

The DsonTest2 harness prints "Press Enter to exit..." and blocks on `std::cin.get()` when it reaches the file-not-found / file-load-failure exit path. Piping empty stdin (`echo "" | .\DsonTest2.exe`) releases it via EOF but the process exits with code 255, not 0.

**Why:** `std::cin.get()` at the bottom of `main()` is a user-facing keypress guard; EOF from a pipe terminates it abruptly.

**How to apply:**
- An exit code 255 from the harness during a piped run is not a test failure — it means the synthetic/inline tests ran fine but the file-load block hit EOF before the final keypress.
- To avoid a hang entirely: use filtered output (`Select-String`) on sections that run before the file-load block (all `Run*` functions registered before `DsonDocument_LoadFromFile`), or pass a known-good file path as argv[1].
- Never report "harness passed" based solely on exit code when running non-interactively. Read the PASS/FAIL lines from the output directly.
