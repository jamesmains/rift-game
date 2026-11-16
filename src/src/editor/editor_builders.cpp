// 1.6.0: shared condition / action builders (#112), search everything (#119), Check all in plain words (#121).
// (Stubs from the 1.6.0 skeleton: the builders work as plain text fields until this file is filled in.)
#include "editor.h"

using namespace ED;

void Editor::openCondBuilder(const std::string& cur, std::function<void(const std::string&)> cb) { openTextPrompt("Condition", cur, cb); }
void Editor::openActionBuilder(const std::string& cur, std::function<void(const std::string&)> cb) { openTextPrompt("Actions", cur, cb); }
std::string Editor::condSentence(const std::string& cond) { return cond; }
std::string Editor::actionSentence(const std::string& actions) { return actions; }
int Editor::condField(int x, int y, int w, std::string& v, bool actions) { (void)actions; ui.textField(x, y, w, v); return 22; }
bool Editor::drawPopupBuilders(InputState& in, bool& close) { (void)in; (void)close; return false; }
