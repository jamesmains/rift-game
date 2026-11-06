// Tutorial guidance (replaces the old one-time tips). A campaign can ship hints.txt: an ordered list of
//   [hint]
//   if=CONDITION        (dialogue conditions, plus mode:explore|combat|shop|menu|journal|levelup|worldmap|craft,
//                        equipped:item, fav:item|spell|any, map:id)
//   title=Training      (optional panel title)
//   text=...            ({action} = that action's key on the device in use, {move}, {click})
// The LAST hint whose condition holds is the one on screen, like quest stages: the panel stays up until the player has
// done what it asks, so it teaches by doing instead of popping up and timing out. The base game ships no hints.
#include "game.h"
#include "../core/audio.h"
#include <algorithm>

using namespace UI;

void Game::loadHints() {
    hintDefs.clear();
    for (auto& sec : mergedConfig("hints.txt")) {
        if (sec.name != "hint") continue;
        HintDef h; h.cond = sec.get("if"); h.title = sec.get("title", "Training"); h.text = sec.get("text");
        if (!h.text.empty()) hintDefs.push_back(h);
    }
    hintShown = -1; hintFade = 0;
}

// {action} -> that action's key on the device in use; {move} -> how to walk; {click} -> " (or click)" with a mouse
std::string Game::keyText(const std::string& raw) const {
    std::string o; size_t i = 0;
    while (i < raw.size()) {
        size_t a = raw.find('{', i); if (a == std::string::npos) { o += raw.substr(i); break; }
        size_t b = raw.find('}', a); if (b == std::string::npos) { o += raw.substr(i); break; }
        o += raw.substr(i, a - i); std::string k = raw.substr(a + 1, b - a - 1), val = "{" + k + "}";
        if (k == "move") val = gPadGlyphs ? "the left stick" : "the arrow keys or WASD";
        else if (k == "click") val = gPadGlyphs ? "" : " (or click)";
        else for (int x = 0; x < A_COUNT; x++) if (k == actionName(x)) {   // just the main key (or pad button): "[Tab]", not "[Tab/I]"
            int code = gPadGlyphs && gKeys.pad[x] ? gKeys.pad[x] : gKeys.key1[x] ? gKeys.key1[x] : gKeys.key2[x];
            val = code ? "[" + std::string(keyName(code)) + "]" : "[" + gKeys.label(x) + "]"; break; }
        o += val; i = b + 1;
    }
    return o;
}

int Game::currentHint() const {
    if (!running) return -1;
    for (int i = (int)hintDefs.size() - 1; i >= 0; i--) if (hintDefs[i].cond.empty() || evalCond(hintDefs[i].cond)) return i;
    return -1;
}

void Game::updateHints(float dt) {
    if (hintDefs.empty()) return;
    int h = currentHint();
    if (h != hintShown) { hintShown = h; hintFade = 0; if (h >= 0) gAudio.play("ui_open"); }
    hintFade = std::min(1.0f, hintFade + dt * 3);
}

// the panel: top centre while exploring, under the unit box in a fight, near the bottom over full-screen menus
int Game::drawHint(Canvas& cv, int y) {
    if (hintShown < 0 || hintShown >= (int)hintDefs.size()) return 0;
    if (mode == GMode::CUTSCENE || mode == GMode::TRANSITION || mode == GMode::DIALOGUE || mode == GMode::MESSAGE || mode == GMode::PAUSE || mode == GMode::CONSOLE) return 0;
    const HintDef& d = hintDefs[hintShown];
    int ts = textScale(), lh = 12 * ts + 1;
    ctx.font->bold = true; int tw = ctx.font->width(d.title) + 14; ctx.font->bold = false;
    int w = ts == 2 ? 620 : 420; auto lines = ctx.font->wrap(keyText(d.text), w - tw - 10, ts);
    int h = 10 + (int)lines.size() * lh; int x = SCREEN_W / 2 - w / 2;
    if (y < 0) y = -y - h;   // a negative y anchors the panel's bottom edge
    float a = hintFade;
    cv.fillRect(x, y, w, h, RGB(18, 30, 26, (uint8_t)(235 * a))); cv.rect(x, y, w, h, Color8{120, 210, 150, (uint8_t)(255 * a)});
    ctx.font->bold = true; ctx.font->draw(cv, *ctx.ts, x + 7, y + 5, d.title, Color8{150, 240, 170, (uint8_t)(255 * a)}); ctx.font->bold = false;
    for (size_t i = 0; i < lines.size(); i++) ctx.font->draw(cv, *ctx.ts, x + tw, y + 5 + (int)i * lh, lines[i], Color8{235, 235, 220, (uint8_t)(255 * a)}, ts);
    return h + 4;
}
