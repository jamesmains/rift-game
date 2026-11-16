// 1.6.0: story tabs - Lore 26 (#123), Epilogue 27 (#124), Credits 28 (#125), Hints 29 (#126), Contradictions 30 (#127).
// (Stubs from the 1.6.0 skeleton.)
#include "editor.h"

using namespace ED;
extern int gEdTop;

void Editor::drawTabStory(InputState& in) { (void)in; ui.text(12, gEdTop + 12, "This tab is not built yet.", DIM); }
bool Editor::tabDirtyStory(int tab) { (void)tab; return false; }
bool Editor::saveDirtyStory() { return true; }
bool Editor::drawPopupStory(InputState& in, bool& close) { (void)in; (void)close; return false; }
