// 1.8.3 (#176-#179): the Desktop-native HUD - the game as a maximized program window on the 90s desktop.
#include "game.h"
#include "tiles.h"
#include <algorithm>
using namespace UI;
void Game::drawNativeHud(Canvas& cv, InputState& in) { (void)cv; (void)in; }
bool nativePartyFits(const Rect& a) { (void)a; return false; }
void Game::drawNativeParty(Canvas& cv, InputState& in) { (void)cv; (void)in; }
