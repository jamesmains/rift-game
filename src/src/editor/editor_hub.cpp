// 1.6.0: the NPC page as a character hub (#109), easier bounties (#111), Test this / previews / names first (#114).
// (Stubs from the 1.6.0 skeleton.)
#include "editor.h"

using namespace ED;

void Editor::requestPlayHere(const std::string& map, int x, int y, const PlayExtra& ex) { (void)map; (void)x; (void)y; (void)ex; say("Test this isn't built yet."); }
bool Editor::drawPopupHub(InputState& in, bool& close) { (void)in; (void)close; return false; }
