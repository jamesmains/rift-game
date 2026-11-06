// Crash handling: on a crash (bad memory access, abort, an uncaught exception, an unhandled Windows exception) write
// saves/crash.log and try an emergency save to saves/crash_recovery.sav. Best effort: the process is already dying.
#pragma once
#include <string>
#include <functional>

void crashInstall(const std::string& saveDir, std::function<std::string()> describeState, std::function<void()> emergencySave);
// Writes the report as if a crash had happened (also what the handlers call). Returns the log path.
std::string crashWriteReport(const std::string& reason);
// A session lock: present while the game runs, removed on a clean exit. Found at start-up = the last run crashed.
bool crashLastRunUnclean(const std::string& saveDir);
void crashSessionBegin(const std::string& saveDir);
void crashSessionEnd(const std::string& saveDir);
