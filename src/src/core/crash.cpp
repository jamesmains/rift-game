#include "crash.h"
#include "version.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static std::string gCrashDir = "saves";
static std::function<std::string()> gDescribe;
static std::function<void()> gEmergencySave;
static volatile int gCrashing = 0;

std::string crashWriteReport(const std::string& reason) {
    std::string path = gCrashDir + "/crash.log";
    FILE* f = fopen(path.c_str(), "a");
    if (!f) return path;
    time_t now = time(nullptr); char when[64] = "?"; if (struct tm* t = localtime(&now)) strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "==== Triffall %s crashed: %s\n", TRIFFALL_VERSION, reason.c_str());
    fprintf(f, "time: %s\n", when);
#ifdef _WIN32
    fprintf(f, "platform: Windows\n");
#elif defined(__APPLE__)
    fprintf(f, "platform: macOS\n");
#else
    fprintf(f, "platform: Linux\n");
#endif
    if (gDescribe) { try { fprintf(f, "%s\n", gDescribe().c_str()); } catch (...) { fprintf(f, "(state unavailable)\n"); } }
    fprintf(f, "Please send this file (and saves/crash_recovery.sav if there is one) with a few words on what you were doing.\n\n");
    fclose(f);
    return path;
}

static void dying(const char* reason) {
    if (gCrashing++) _Exit(3);   // crashed again while reporting: just go
    crashWriteReport(reason);
    if (gEmergencySave) { try { gEmergencySave(); } catch (...) {} }
}
static void onSignal(int sig) {
    const char* r = sig == SIGSEGV ? "SIGSEGV (bad memory access)" : sig == SIGABRT ? "SIGABRT (abort)" : sig == SIGFPE ? "SIGFPE (arithmetic error)" : sig == SIGILL ? "SIGILL (illegal instruction)" : "signal";
    dying(r);
    signal(sig, SIG_DFL); raise(sig);
}
static void onTerminate() {
    std::string why = "uncaught exception";
    if (auto ep = std::current_exception()) { try { std::rethrow_exception(ep); } catch (const std::exception& e) { why += std::string(": ") + e.what(); } catch (...) {} }
    dying(why.c_str());
    std::abort();
}
#ifdef _WIN32
static LONG WINAPI onWinException(EXCEPTION_POINTERS* ep) {
    char buf[96]; snprintf(buf, sizeof buf, "Windows exception 0x%08lX", ep && ep->ExceptionRecord ? (unsigned long)ep->ExceptionRecord->ExceptionCode : 0ul);
    dying(buf);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void crashInstall(const std::string& saveDir, std::function<std::string()> describeState, std::function<void()> emergencySave) {
    gCrashDir = saveDir; gDescribe = describeState; gEmergencySave = emergencySave;
    signal(SIGSEGV, onSignal); signal(SIGABRT, onSignal); signal(SIGFPE, onSignal); signal(SIGILL, onSignal);
    std::set_terminate(onTerminate);
#ifdef _WIN32
    SetUnhandledExceptionFilter(onWinException);
#endif
}

static std::string lockPath(const std::string& dir) { return dir + "/session.lock"; }
bool crashLastRunUnclean(const std::string& saveDir) { std::error_code ec; return std::filesystem::exists(lockPath(saveDir), ec); }
void crashSessionBegin(const std::string& saveDir) { if (FILE* f = fopen(lockPath(saveDir).c_str(), "w")) { fprintf(f, "Triffall %s is running. If this file is still here when the game starts, the last session did not end cleanly.\n", TRIFFALL_VERSION); fclose(f); } }
void crashSessionEnd(const std::string& saveDir) { std::error_code ec; std::filesystem::remove(lockPath(saveDir), ec); }
