// orientation:
// Standalone dynamic-load regression test for the DsonParser C ABI threading
// contract (DsonParser 1.6.0). UNLIKE DsonTest2, this target does NOT link
// DsonParser.lib; it loads DsonParser.dll at runtime via LoadLibrary +
// GetProcAddress, exactly the way the UE plugin consumes it (FPlatformProcess::
// GetDllHandle / GetDllExport). It exists to reproduce the one scenario the
// .lib-linked harness structurally cannot: worker threads that PREDATE the DLL
// load calling into the DLL. That is the case that broke an earlier file-scope
// thread_local (TLS not initialised for a pre-existing thread under a dynamically
// loaded DLL), and that the function-local thread_local last-error slot fixes.
//
// Responsibilities:
// - Start two worker threads BEFORE LoadLibrary (so they predate the DLL load).
// - After the DLL is mapped, each worker resolves the exports and performs one
//   load on its own thread, then reads DsonParser_GetLastError() on that thread.
// - Assert: the failing-load worker sees its OWN non-empty error; the
//   succeeding-load worker sees ""; the two never share an error (per-thread
//   isolation) -- all under the real dynamic-load model.
// - Exit code 0 on PASS, non-zero on FAIL, so a build/verify step can gate on it.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

namespace {

// C ABI signatures, resolved at runtime. We deliberately do NOT include
// DsonParserAPI.h or link the import lib -- the point is to depend on the DLL only
// at runtime, like the dynamic-load consumer does.
using DsonHandle       = void*;
using Create_t         = DsonHandle (*)();
using LoadFromString_t = int (*)(DsonHandle, const char*);
using GetLastErr_t     = const char* (*)();
using Destroy_t        = void (*)(DsonHandle);
using GetVersion_t     = const char* (*)();

struct WorkerResult {
    int loadResult = -999;
    std::string lastError;
    bool resolveFailed = false;
};

} // namespace

int main() {
    std::atomic<bool> dllReady{ false };
    HMODULE dll = nullptr;   // written by main before dllReady=release; read by
                             // workers after dllReady=acquire (happens-before).
    WorkerResult a;          // worker that performs a FAILING load
    WorkerResult b;          // worker that performs a SUCCEEDING load

    // Drives one load + GetLastError entirely on the calling worker thread, once the
    // DLL is mapped. Resolves exports by name at runtime (no import lib, no header).
    auto run = [&dllReady, &dll](const char* json, WorkerResult& out) {
        while (!dllReady.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        auto create  = reinterpret_cast<Create_t>(GetProcAddress(dll, "DsonDocument_Create"));
        auto loadStr = reinterpret_cast<LoadFromString_t>(GetProcAddress(dll, "DsonDocument_LoadFromString"));
        auto getErr  = reinterpret_cast<GetLastErr_t>(GetProcAddress(dll, "DsonParser_GetLastError"));
        auto destroy = reinterpret_cast<Destroy_t>(GetProcAddress(dll, "DsonDocument_Destroy"));
        if (!create || !loadStr || !getErr || !destroy) {
            out.resolveFailed = true;
            return;
        }
        DsonHandle h = create();
        out.loadResult = loadStr(h, json);
        const char* e = getErr();
        out.lastError = e ? e : "";
        destroy(h);
    };

    // CRITICAL: start both workers BEFORE LoadLibrary, so they predate the DLL load
    // -- the exact condition that broke a file-scope thread_local under GetDllHandle.
    std::thread tA(run, "{ this is invalid json", std::ref(a));
    std::thread tB(run, "{}", std::ref(b));

    dll = LoadLibraryW(L"DsonParser.dll");
    const bool loaded = (dll != nullptr);
    const DWORD loadErr = loaded ? 0u : ::GetLastError();
    dllReady.store(true, std::memory_order_release);  // release workers (even on failure, so they exit)

    tA.join();
    tB.join();

    std::string version = "(unresolved)";
    if (loaded) {
        auto getVer = reinterpret_cast<GetVersion_t>(GetProcAddress(dll, "DsonParser_GetVersion"));
        if (getVer) { const char* v = getVer(); version = v ? v : ""; }
        FreeLibrary(dll);
    }

    const bool passLoad    = loaded;
    const bool passResolve = !a.resolveFailed && !b.resolveFailed;
    const bool passA       = (a.loadResult != 0) && !a.lastError.empty();  // pre-existing thread sees its OWN error
    const bool passB       = (b.loadResult == 0) && b.lastError.empty();   // success -> empty on its own thread
    const bool passIso     = (a.lastError != b.lastError);                 // per-thread isolation under dynamic load

    std::cout << "DsonLoadTest -- dynamic load, threads predate DLL load (DsonParser " << version << ")\n";
    if (!loaded) {
        std::cout << "  LoadLibrary(DsonParser.dll): FAIL (Win32 error " << loadErr << ")\n";
    } else {
        std::cout << "  LoadLibrary after thread creation: OK\n";
    }
    std::cout << "  Worker A (invalid JSON): result=" << a.loadResult << " error=\"" << a.lastError << "\"\n";
    std::cout << "  Worker B (valid JSON):   result=" << b.loadResult << " error=\"" << b.lastError << "\"\n";
    std::cout << "  [1] DLL loaded dynamically:                  " << (passLoad ? "PASS" : "FAIL") << "\n";
    std::cout << "  [2] exports resolved on pre-existing thread: " << (passResolve ? "PASS" : "FAIL") << "\n";
    std::cout << "  [3] A (pre-existing thread) sees its error:  " << (passA ? "PASS" : "FAIL") << "\n";
    std::cout << "  [4] B sees empty on success:                 " << (passB ? "PASS" : "FAIL") << "\n";
    std::cout << "  [5] per-thread isolation (A != B):           " << (passIso ? "PASS" : "FAIL") << "\n";

    const bool ok = passLoad && passResolve && passA && passB && passIso;
    std::cout << (ok ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return ok ? 0 : 1;
}
