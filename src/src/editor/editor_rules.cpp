// 1.6.0: rules tabs - Dungeon styles 31 (#128), Healers 32 (#129), Combat rules 33 (#130), Formulas 34 (#131).
// (Stubs from the 1.6.0 skeleton.)
#include "editor.h"

using namespace ED;
extern int gEdTop;

void Editor::drawTabRules(InputState& in) { (void)in; ui.text(12, gEdTop + 12, "This tab is not built yet.", DIM); }
bool Editor::tabDirtyRules(int tab) { (void)tab; return false; }
bool Editor::saveDirtyRules() { return true; }
bool Editor::drawPopupRules(InputState& in, bool& close) { (void)in; (void)close; return false; }
