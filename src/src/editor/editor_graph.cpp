// Editor dialogue node graph: a live view of the open dialogue file (te.lines) in the Dialogue tab.
// The text stays the source of truth: every change made here (new node, relinked option) is written into te.lines.
#include "editor.h"
#include <algorithm>
#include <functional>
#include <deque>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ED;

namespace {
const int NODE_W = 150, COL_GAP = 60, ROW_GAP = 16, MARGIN = 16;
const int HEAD_H = 14, LINE_H = 12, ROW_H = 13, TOOLBAR_H = 20, STRIP_H = 112;
const Color8 E_OPT = RGB(90, 200, 110), E_NEXT = RGB(90, 160, 255), E_COND = RGB(235, 190, 70), E_BAD = RGB(220, 70, 70);
const Color8 E_JUMP = RGB(190, 130, 255), E_END = RGB(150, 150, 160);

// shorten s to fit maxW pixels (adds "..")
std::string fitText(Font* f, const std::string& s, int maxW) {
    if (f->width(s) <= maxW) return s;
    int ell = f->width(".."), wsum = 0; std::string out;
    for (char c : s) { int cw = f->width(std::string(1, c)); if (wsum + cw + ell > maxW) break; wsum += cw; out += c; }
    return out + "..";
}
// orthogonal segment, optionally dashed (dash phase carried in 'ph' so a polyline dashes evenly)
void seg(Canvas& cv, int x0, int y0, int x1, int y1, Color8 c, bool dashed, int& ph) {
    if (!dashed) { if (y0 == y1) cv.fillRect(std::min(x0, x1), y0, std::abs(x1 - x0) + 1, 1, c); else cv.fillRect(x0, std::min(y0, y1), 1, std::abs(y1 - y0) + 1, c); return; }
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0)), sx = x1 > x0 ? 1 : x1 < x0 ? -1 : 0, sy = y1 > y0 ? 1 : y1 < y0 ? -1 : 0;
    for (int i = 0; i <= n; i++, ph++) if (ph % 7 < 4) cv.fillRect(x0 + sx * i, y0 + sy * i, 1, 1, c);
}
void poly(Canvas& cv, const std::vector<std::pair<int, int>>& pts, Color8 c, bool dashed) {
    int ph = 0; for (size_t i = 1; i < pts.size(); i++) seg(cv, pts[i - 1].first, pts[i - 1].second, pts[i].first, pts[i].second, c, dashed, ph);
}
void arrow(Canvas& cv, int x, int y, Color8 c) { for (int i = 0; i < 4; i++) cv.fillRect(x - 1 - i, y - i, 1, 2 * i + 1, c); }
bool closingKey(const std::string& k) { return k == "combat" || k == "shop" || k == "teleport" || k == "dialogue" || k == "win" || k == "cutscene" || k == "craft" || k == "board" || k == "travel" || k == "respec" || k == "healer"; }
}

void Editor::dlgGraphParse() {
    dlgG.clear(); dlgGIdx.clear();
    DlgGNode* cur = nullptr;
    for (size_t i = 0; i < te.lines.size(); i++) {
        std::string l = trim(te.lines[i]);
        if (l.empty() || l[0] == '#') continue;
        if (l.size() >= 2 && l[0] == '[' && l.back() == ']') {
            DlgGNode n; n.id = trim(l.substr(1, l.size() - 2)); n.line = (int)i;
            if (dlgGIdx.count(n.id)) n.dup = true; else dlgGIdx[n.id] = (int)dlgG.size();
            dlgG.push_back(n); cur = &dlgG.back(); continue;
        }
        if (!cur) continue;
        size_t eq = l.find('='); if (eq == std::string::npos) continue;
        std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        if (k == "speaker") cur->speaker = v;
        else if (k == "text") cur->text.push_back(v);
        else if (k == "option") {
            DlgGOpt o; o.line = (int)i; auto p = split(v, '|');
            o.label = trim(p[0]); o.hasPipe = p.size() > 1; if (p.size() > 1) o.target = trim(p[1]); if (p.size() > 2) o.cond = trim(p[2]);
            cur->opts.push_back(o);
        } else if (k == "next") { cur->next = v; cur->nextLine = (int)i; }
        else {
            if (k == "dialogue") cur->jumpFile = v;
            cur->acts.push_back(k + "=" + v);
        }
    }
}

void Editor::dlgGraphLayout(bool keepDragged) {
    int n = (int)dlgG.size(); if (!n) return;
    auto tgt = [&](const std::string& t) { if (t.empty()) return -1; auto it = dlgGIdx.find(t); return it == dlgGIdx.end() ? -1 : it->second; };
    // sizes + incoming edges
    for (auto& g : dlgG) {
        int body = (g.speaker.empty() ? 0 : 1) + 1 + (g.acts.empty() ? 0 : 1);
        int rows = (int)g.opts.size() + (g.next.empty() ? 0 : 1) + (g.jumpFile.empty() ? 0 : 1);
        g.w = NODE_W; g.h = HEAD_H + 3 + body * LINE_H + 2 + rows * ROW_H + 3; g.depth = -1; g.reached = false;
    }
    for (int i = 0; i < n; i++) { for (auto& o : dlgG[i].opts) { int t = tgt(o.target); if (t >= 0) dlgG[t].reached = true; } int t = tgt(dlgG[i].next); if (t >= 0) dlgG[t].reached = true; }
    // BFS bands: [start] (or the first node), then [camp]
    std::vector<int> roots; int s = tgt("start"); roots.push_back(s >= 0 ? s : 0);
    int c = tgt("camp"); if (c >= 0 && c != roots[0]) roots.push_back(c);
    std::vector<int> band(n, -1); int maxDepth = 0, bandTop = MARGIN, maxBottom = MARGIN;
    std::vector<std::vector<std::vector<int>>> cols;
    for (size_t b = 0; b < roots.size(); b++) {
        if (dlgG[roots[b]].depth >= 0) continue;
        std::deque<int> q{roots[b]}; dlgG[roots[b]].depth = 0; std::vector<std::vector<int>> bc;
        while (!q.empty()) {
            int i = q.front(); q.pop_front(); int d = dlgG[i].depth;
            if ((int)bc.size() <= d) bc.resize(d + 1); bc[d].push_back(i); maxDepth = std::max(maxDepth, d);
            auto visit = [&](const std::string& t) { int j = tgt(t); if (j >= 0 && dlgG[j].depth < 0) { dlgG[j].depth = d + 1; q.push_back(j); } };
            for (auto& o : dlgG[i].opts) visit(o.target); visit(dlgG[i].next);
        }
        int bottom = bandTop;
        for (size_t d = 0; d < bc.size(); d++) { int yy = bandTop; for (int i : bc[d]) { dlgG[i].x = MARGIN + (int)d * (NODE_W + COL_GAP); dlgG[i].y = yy; yy += dlgG[i].h + ROW_GAP; } bottom = std::max(bottom, yy); }
        bandTop = bottom + 24; maxBottom = bottom;
    }
    (void)maxBottom;
    // everything not reached from a root (and duplicates) goes into a final column
    int yy = MARGIN, lastCol = MARGIN + (maxDepth + 1) * (NODE_W + COL_GAP);
    for (int i = 0; i < n; i++) if (dlgG[i].depth < 0) { dlgG[i].x = lastCol; dlgG[i].y = yy; yy += dlgG[i].h + ROW_GAP; }
    if (keepDragged) { auto& pm = dlgGPos[te.path]; for (auto& g : dlgG) { auto it = pm.find(g.dup ? g.id + "#" + std::to_string(g.line) : g.id); if (it != pm.end()) { g.x = it->second.first; g.y = it->second.second; } } }
}

void Editor::dlgGraphJump(int node) {
    if (node < 0 || node >= (int)dlgG.size()) return;
    dlgGraphMode = false; ui.blur(); dlgGFocusText = true;
    te.cy = clampi(dlgG[node].line, 0, (int)te.lines.size() - 1); te.cx = 0; te.ax = -1; te.ay = -1; te.scroll = std::max(0, te.cy - 6);
}

void Editor::dlgGraphAddNode() {
    int k = 1; while (dlgGIdx.count("node_" + std::to_string(k))) k++;
    std::string id = "node_" + std::to_string(k);
    const DlgGNode* sel = dlgGSel >= 0 && dlgGSel < (int)dlgG.size() ? &dlgG[dlgGSel] : nullptr;
    std::string speaker = sel ? sel->speaker : (dlgGIdx.count("start") ? dlgG[dlgGIdx["start"]].speaker : "");
    te.snapshot();
    if (!te.lines.empty() && !trim(te.lines.back()).empty()) te.lines.push_back("");
    te.lines.push_back("[" + id + "]");
    if (!speaker.empty()) te.lines.push_back("speaker=" + speaker);
    te.lines.push_back("text=...");
    te.lines.push_back(sel ? "option=Back.|" + sel->id : "option=Goodbye.|end");
    te.dirty = true; te.cy = clampi(te.cy, 0, (int)te.lines.size() - 1); te.cx = std::min(te.cx, (int)te.lines[te.cy].size()); te.ay = -1;
    dlgGSelId = id; say("Added [" + id + "] at the end of the file.");
}

void Editor::dlgGraphRetarget(int node, int opt, const std::string& target) {
    if (node < 0 || node >= (int)dlgG.size()) return;
    int ln = opt == -2 ? dlgG[node].nextLine : (opt >= 0 && opt < (int)dlgG[node].opts.size() ? dlgG[node].opts[opt].line : -1);
    if (ln < 0 || ln >= (int)te.lines.size()) return;
    std::string& L = te.lines[ln]; size_t eq = L.find('='); if (eq == std::string::npos) return;
    std::string head = L.substr(0, eq + 1), val = L.substr(eq + 1), out;
    if (opt == -2) out = head + target;
    else {
        auto p = split(val, '|');
        if (p.size() < 2) out = head + val + "|" + target;
        else { p[1] = target; out = head; for (size_t i = 0; i < p.size(); i++) out += (i ? "|" : "") + p[i]; }
    }
    if (out == L) return;
    te.snapshot(); L = out; te.dirty = true;
    say(fmt("Line %d now leads to [%s].", ln + 1, target.c_str()));
}

void Editor::drawDialogueGraph(int x, int y, int w, int h, InputState& in) {
    Canvas& cv = *ui.cv;
    bool live = !ui.modal && pop == P_NONE;
    // ---- reparse when the text (or file) changed ----
    std::string all = te.path + "\n" + te.text(); size_t hs = std::hash<std::string>{}(all);
    if (te.path != dlgGPath) { dlgGPath = te.path; dlgGPanX = dlgGPanY = 0; dlgGSel = -1; dlgGSelId.clear(); dlgGDrag = dlgGLinkNode = -1; dlgGHash = 0; }
    if (hs != dlgGHash) { dlgGHash = hs; dlgGraphParse(); dlgGraphLayout(true); }
    // keep the selection on the same node across reparses
    if (!dlgGSelId.empty()) { if (dlgGSel < 0 || dlgGSel >= (int)dlgG.size() || dlgG[dlgGSel].id != dlgGSelId) { auto it = dlgGIdx.find(dlgGSelId); dlgGSel = it == dlgGIdx.end() ? -1 : it->second; } }
    else dlgGSel = -1;
    auto tgt = [&](const std::string& t) { if (t.empty()) return -1; auto it = dlgGIdx.find(t); return it == dlgGIdx.end() ? -1 : it->second; };
    auto posKey = [&](const DlgGNode& g) { return g.dup ? g.id + "#" + std::to_string(g.line) : g.id; };
    int unreached = 0, missing = 0;
    for (auto& g : dlgG) { if (!g.reached && g.id != "start" && g.id != "camp") unreached++; for (auto& o : g.opts) if (o.target != "end" && tgt(o.target) < 0) missing++; if (!g.next.empty() && g.next != "end" && tgt(g.next) < 0) missing++; }

    // ---- toolbar ----
    int gx = x, gy = y + TOOLBAR_H, gw = w, gh = h - TOOLBAR_H - STRIP_H;
    cv.fillRect(x, y, w, TOOLBAR_H, PANEL);
    if (ui.button(x + 2, y + 2, 64, 16, "Re-layout")) { dlgGPos.erase(te.path); dlgGraphLayout(false); dlgGPanX = dlgGPanY = 0; }
    if (ui.hover(x + 2, y + 2, 64, 16)) ui.tip("Forget dragged positions and lay the nodes out again");
    if (ui.button(x + 70, y + 2, 54, 16, "+ node", true, false, OK)) {
        dlgGraphAddNode(); dlgGHash = std::hash<std::string>{}(te.path + "\n" + te.text()); dlgGraphParse(); dlgGraphLayout(true);
        auto it = dlgGIdx.find(dlgGSelId); dlgGSel = it == dlgGIdx.end() ? -1 : it->second;
        if (dlgGSel >= 0) { dlgGPanX = dlgG[dlgGSel].x - gw / 2 + NODE_W / 2; dlgGPanY = dlgG[dlgGSel].y - gh / 3; }
    }
    if (ui.hover(x + 70, y + 2, 54, 16)) ui.tip("Add a new [node_N] at the end of the file (with a Back option to the selected node)");
    std::string stat = fmt("%d nodes", (int)dlgG.size());
    if (unreached) stat += fmt(", %d unreached", unreached);
    if (missing) stat += fmt(", %d bad link%s", missing, missing == 1 ? "" : "s");
    ui.text(x + 132, y + 4, stat, missing ? DANGER : unreached ? ACCENT2 : DIM);

    // ---- input ----
    bool inArea = live && ui.hover(gx, gy, gw, gh);
    auto nodeAt = [&](int mx, int my) { for (int k = (int)dlgG.size(); k >= 0; k--) { int i = k == (int)dlgG.size() ? dlgGSel : k; if (i < 0 || i >= (int)dlgG.size() || (k < (int)dlgG.size() && i == dlgGSel)) continue; auto& g = dlgG[i]; int sx = gx + g.x - dlgGPanX, sy = gy + g.y - dlgGPanY; if (mx >= sx && my >= sy && mx < sx + g.w && my < sy + g.h) return i; } return -1; };
    auto rowsTop = [&](const DlgGNode& g) { return g.y + HEAD_H + 3 + ((g.speaker.empty() ? 0 : 1) + 1 + (g.acts.empty() ? 0 : 1)) * LINE_H + 2; };
    auto rowAt = [&](int i, int my) { auto& g = dlgG[i]; int ry = gy + rowsTop(g) - dlgGPanY; int r = (my - ry) / ROW_H; if (my < ry) return -1; if (r < (int)g.opts.size()) return r; if (r == (int)g.opts.size() && !g.next.empty()) return -2; return -1; };
    if (!live) { dlgGDrag = dlgGLinkNode = -1; dlgGPanning = false; }
    if (inArea && !ui.mouseUsed) {
        if (in.wheel) { if (in.shift()) dlgGPanX -= in.wheel * 40; else dlgGPanY -= in.wheel * 40; }
        if (in.mousePressed[1] || in.mousePressed[2]) dlgGPanning = true;
        if (in.mousePressed[0]) {
            ui.mouseUsed = true; ui.blur();
            int i = nodeAt(in.mouseX, in.mouseY);
            if (i >= 0) {
                int r = in.shift() ? rowAt(i, in.mouseY) : -1;
                if (in.shift() && (r >= 0 || r == -2)) { dlgGLinkNode = i; dlgGLinkOpt = r; }
                else {
                    bool dbl = dlgGLastClick == i && ui.time - dlgGLastClickT < 0.35f;
                    dlgGSel = i; dlgGSelId = dlgG[i].id; dlgGLastClick = i; dlgGLastClickT = ui.time;
                    if (dbl) { dlgGraphJump(i); return; }
                    dlgGDrag = i; dlgGDragMoved = false; dlgGDragOX = in.mouseX - (gx + dlgG[i].x - dlgGPanX); dlgGDragOY = in.mouseY - (gy + dlgG[i].y - dlgGPanY);
                }
            } else { dlgGSel = -1; dlgGSelId.clear(); dlgGPanning = true; dlgGLastClick = -1; }
        }
    }
    if (dlgGPanning) { if (!(in.mouseDown[0] || in.mouseDown[1] || in.mouseDown[2])) dlgGPanning = false; else { dlgGPanX -= in.mouseDX; dlgGPanY -= in.mouseDY; } }
    if (dlgGDrag >= 0 && dlgGDrag < (int)dlgG.size()) {
        if (!in.mouseDown[0]) dlgGDrag = -1;
        else if (in.mouseDX || in.mouseDY) {
            auto& g = dlgG[dlgGDrag]; g.x = in.mouseX - dlgGDragOX - gx + dlgGPanX; g.y = in.mouseY - dlgGDragOY - gy + dlgGPanY; dlgGDragMoved = true;
            dlgGPos[te.path][posKey(g)] = {g.x, g.y};
        }
    } else dlgGDrag = -1;
    if (dlgGLinkNode >= 0 && !in.mouseDown[0]) {   // drop: relink to the node under the mouse
        int t = nodeAt(in.mouseX, in.mouseY);
        if (t >= 0 && live && ui.hover(gx, gy, gw, gh) && dlgGLinkNode < (int)dlgG.size()) dlgGraphRetarget(dlgGLinkNode, dlgGLinkOpt, dlgG[t].id);
        dlgGLinkNode = -1;
    }
    if (live && !ui.typing() && dlgGSel >= 0) { if (in.pressed[K_ENTER] && !in.ctrl()) { dlgGraphJump(dlgGSel); return; } }
    if (live && !ui.typing() && in.ctrl()) {
        if (in.pressed[K_Z] && !te.undo.empty()) { te.lines = te.undo.back(); te.undo.pop_back(); if (te.lines.empty()) te.lines.push_back(""); te.cy = clampi(te.cy, 0, (int)te.lines.size() - 1); te.cx = std::min(te.cx, (int)te.lines[te.cy].size()); te.ay = -1; te.dirty = true; say("Undo."); }
        if (in.pressed[K_S] && te.save()) { changed = true; say("Saved " + te.path); }
    }
    // keep some of the graph on screen
    if (!dlgG.empty()) {
        int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30);
        for (auto& g : dlgG) { x0 = std::min(x0, g.x); y0 = std::min(y0, g.y); x1 = std::max(x1, g.x + g.w); y1 = std::max(y1, g.y + g.h); }
        dlgGPanX = clampi(dlgGPanX, x0 - gw + 80, std::max(x0 - gw + 80, x1 - 80)); dlgGPanY = clampi(dlgGPanY, y0 - gh + 60, std::max(y0 - gh + 60, y1 - 60));
    }

    // ---- canvas ----
    cv.setClip({gx, gy, gw, gh});
    cv.fillRect(gx, gy, gw, gh, FIELD);
    for (int yy = ((-dlgGPanY) % 24 + 24) % 24; yy < gh; yy += 24) for (int xx = ((-dlgGPanX) % 24 + 24) % 24; xx < gw; xx += 24) cv.fillRect(gx + xx, gy + yy, 1, 1, RGB(48, 52, 62));
    if (te.path.empty() || dlgG.empty()) {
        ui.text(gx + 12, gy + 12, te.path.empty() ? "No file open." : "No [nodes] in this file yet - press + node, or switch to Text.", DIM);
    }
    auto S = [&](int wx, int wy) { return std::make_pair(gx + wx - dlgGPanX, gy + wy - dlgGPanY); };
    // edges
    auto drawEdges = [&](int i, bool hi) {
        auto& g = dlgG[i]; int rt = rowsTop(g);
        int nrow = (int)g.opts.size() + (g.next.empty() ? 0 : 1);
        for (int r = 0; r < nrow; r++) {
            bool isNext = r == (int)g.opts.size();
            const std::string& t = isNext ? g.next : g.opts[r].target;
            bool cond = !isNext && !g.opts[r].cond.empty();
            auto a = S(g.x + g.w, rt + r * ROW_H + 6);
            if (t == "end") {
                cv.fillRect(a.first, a.second, 8, 1, E_END);
                int bw = font->width("end") + 6; cv.fillRect(a.first + 8, a.second - 5, bw, 11, RGB(50, 52, 60)); cv.rect(a.first + 8, a.second - 5, bw, 11, E_END);
                font->draw(cv, *ts, a.first + 11, a.second - 5, "end", E_END);
                continue;
            }
            int j = tgt(t);
            if (j < 0) {
                cv.fillRect(a.first, a.second, 10, 1, E_BAD);
                std::string lbl = "?" + fitText(font, t.empty() ? std::string("no target") : t, 70);
                int bw = font->width(lbl) + 6; cv.fillRect(a.first + 10, a.second - 5, bw, 11, RGB(70, 30, 30)); cv.rect(a.first + 10, a.second - 5, bw, 11, E_BAD);
                font->draw(cv, *ts, a.first + 13, a.second - 5, lbl, E_BAD);
                continue;
            }
            Color8 col = isNext ? E_NEXT : cond ? E_COND : E_OPT;
            if (dlgGSel >= 0 && !hi) col.a = 90;
            auto& d = dlgG[j]; auto b = S(d.x, d.y + 7);
            int lane = a.first + 8 + std::min(r, 9) * 5;
            std::vector<std::pair<int, int>> pts;
            if (b.first - 12 > lane) pts = {a, {lane, a.second}, {lane, b.second}, {b.first, b.second}};
            else { int above = S(0, d.y - 7).second; pts = {a, {lane, a.second}, {lane, above}, {b.first - 12, above}, {b.first - 12, b.second}, {b.first, b.second}}; }
            poly(cv, pts, col, cond);
            arrow(cv, b.first, b.second, col);
        }
        if (!g.jumpFile.empty()) {
            auto a = S(g.x + g.w, rt + nrow * ROW_H + 6);
            cv.fillRect(a.first, a.second, 8, 1, E_JUMP);
            std::string lbl = fitText(font, g.jumpFile, 90) + " >"; int bw = font->width(lbl) + 6;
            cv.fillRect(a.first + 8, a.second - 5, bw, 11, RGB(50, 36, 70)); cv.rect(a.first + 8, a.second - 5, bw, 11, E_JUMP);
            font->draw(cv, *ts, a.first + 11, a.second - 5, lbl, E_JUMP);
        }
    };
    for (int i = 0; i < (int)dlgG.size(); i++) if (i != dlgGSel) drawEdges(i, dlgGSel < 0);
    // edges into the selected node are highlighted too
    for (int i = 0; i < (int)dlgG.size(); i++) if (dlgGSel >= 0 && i != dlgGSel) { bool into = false; for (auto& o : dlgG[i].opts) if (tgt(o.target) == dlgGSel) into = true; if (tgt(dlgG[i].next) == dlgGSel) into = true; if (into) drawEdges(i, true); }
    if (dlgGSel >= 0) drawEdges(dlgGSel, true);
    // nodes
    int hoverNode = inArea ? nodeAt(in.mouseX, in.mouseY) : -1;
    for (int k = 0; k <= (int)dlgG.size(); k++) {   // the selected node is drawn last (on top)
        int i = k == (int)dlgG.size() ? dlgGSel : k; if (i < 0 || i >= (int)dlgG.size() || (k < (int)dlgG.size() && i == dlgGSel)) continue;
        auto& g = dlgG[i]; auto p = S(g.x, g.y); int sx = p.first, sy = p.second;
        if (sx > gx + gw || sy > gy + gh || sx + g.w < gx || sy + g.h < gy) continue;
        bool isStart = g.id == "start" && !g.dup, isCamp = g.id == "camp" && !g.dup, warnUnreached = !g.reached && !isStart && !isCamp;
        Color8 headCol = g.dup ? RGB(120, 40, 40) : isStart ? RGB(40, 110, 64) : isCamp ? RGB(90, 60, 130) : warnUnreached ? RGB(110, 80, 30) : RGB(52, 58, 72);
        Color8 border = i == dlgGSel ? ACCENT : g.dup ? DANGER : warnUnreached ? ACCENT2 : i == hoverNode ? DIM : BORDER;
        cv.fillRect(sx + 3, sy + 3, g.w, g.h, RGB(0, 0, 0, 90));
        cv.fillRect(sx, sy, g.w, g.h, PANEL);
        cv.fillRect(sx, sy, g.w, HEAD_H, headCol);
        cv.rect(sx, sy, g.w, g.h, border); if (i == dlgGSel) cv.rect(sx - 1, sy - 1, g.w + 2, g.h + 2, ACCENT);
        std::string head = (g.id.empty() ? std::string("(empty id)") : g.id) + (g.dup ? " (dup)" : "");
        if (isStart) head = "> " + head;
        font->draw(cv, *ts, sx + 4, sy + 1, fitText(font, head, g.w - 34), C::White);
        std::string ln = std::to_string(g.line + 1); font->draw(cv, *ts, sx + g.w - 4 - font->width(ln), sy + 1, ln, Color8{255, 255, 255, 120});
        int yy = sy + HEAD_H + 3;
        if (!g.speaker.empty()) { font->draw(cv, *ts, sx + 4, yy, fitText(font, g.speaker, g.w - 8), ACCENT); yy += LINE_H; }
        font->draw(cv, *ts, sx + 4, yy, g.text.empty() ? std::string("(no text)") : fitText(font, g.text[0], g.w - 8), g.text.empty() ? DIM : TEXT); yy += LINE_H;
        if (!g.acts.empty()) {
            std::string a; for (auto& s : g.acts) { std::string k = s.substr(0, s.find('=')); if (!a.empty()) a += " "; a += k; }
            bool closes = false; for (auto& s : g.acts) if (closingKey(s.substr(0, s.find('=')))) closes = true;
            font->draw(cv, *ts, sx + 4, yy, fitText(font, "* " + a, g.w - 8), closes ? E_JUMP : ACCENT2); yy += LINE_H;
        }
        yy += 2; cv.fillRect(sx + 1, yy - 2, g.w - 2, 1, BORDER);
        for (size_t r = 0; r < g.opts.size(); r++) {
            auto& o = g.opts[r]; int ry = yy + (int)r * ROW_H;
            bool rh = i == hoverNode && in.mouseY >= ry && in.mouseY < ry + ROW_H;
            if (rh) cv.fillRect(sx + 1, ry, g.w - 2, ROW_H, HOVER);
            Color8 c = (o.target != "end" && tgt(o.target) < 0) ? E_BAD : !o.cond.empty() ? E_COND : E_OPT;
            cv.fillRect(sx + 4, ry + 5, 3, 3, c);
            font->draw(cv, *ts, sx + 10, ry + 1, fitText(font, o.label.empty() ? std::string("(no label)") : o.label, g.w - 14), TEXT);
            if (rh) ui.tip(fitText(font, o.label, 200) + "  ->  " + (o.target.empty() ? std::string("(no target: needs Label|target)") : o.target) + (o.cond.empty() ? "" : "   if " + o.cond) + "   (shift-drag to relink)");
        }
        if (!g.next.empty()) {
            int ry = yy + (int)g.opts.size() * ROW_H;
            bool rh = i == hoverNode && in.mouseY >= ry && in.mouseY < ry + ROW_H;
            if (rh) { cv.fillRect(sx + 1, ry, g.w - 2, ROW_H, HOVER); ui.tip("next=" + g.next + "  (auto-advance; shift-drag to relink)"); }
            font->draw(cv, *ts, sx + 10, ry + 1, "next", E_NEXT);
        }
        if (!g.jumpFile.empty()) { int ry = yy + ((int)g.opts.size() + (g.next.empty() ? 0 : 1)) * ROW_H; font->draw(cv, *ts, sx + 10, ry + 1, fitText(font, "dialogue: " + g.jumpFile, g.w - 14), E_JUMP); }
        if (i == hoverNode && in.mouseY < sy + HEAD_H) ui.tip(g.dup ? "duplicate node id - the game uses the first one" : warnUnreached ? "nothing in this file leads here (ok if another dialogue jumps here)" : "double-click or Enter: edit this node as text");
    }
    // relink drag line
    if (dlgGLinkNode >= 0 && dlgGLinkNode < (int)dlgG.size()) {
        auto& g = dlgG[dlgGLinkNode]; int r = dlgGLinkOpt == -2 ? (int)g.opts.size() : dlgGLinkOpt;
        auto a = S(g.x + g.w, rowsTop(g) + r * ROW_H + 6); int ph = 0;
        seg(cv, a.first, a.second, in.mouseX, a.second, C::White, true, ph); seg(cv, in.mouseX, a.second, in.mouseX, in.mouseY, C::White, true, ph);
        int t = nodeAt(in.mouseX, in.mouseY); if (t >= 0) { auto q = S(dlgG[t].x, dlgG[t].y); cv.rect(q.first - 2, q.second - 2, dlgG[t].w + 4, dlgG[t].h + 4, C::White); }
    }
    ui.text(gx + gw - 4 - font->width("drag: move   shift-drag option: relink   right-drag/wheel: pan"), gy + gh - 16, "drag: move   shift-drag option: relink   right-drag/wheel: pan", Color8{255, 255, 255, 100});
    cv.resetClip();

    // ---- info strip ----
    int iy = y + h - STRIP_H;
    cv.fillRect(x, iy, w, STRIP_H, PANEL2); cv.hline(x, iy, w, BORDER);
    cv.setClip({x + 1, iy + 1, w - 2, STRIP_H - 2});
    if (dlgGSel < 0 || dlgGSel >= (int)dlgG.size()) {
        ui.text(x + 6, iy + 5, "Click a node to see it here. Double-click (or Enter) edits it in the text view.", DIM);
        int ly = iy + 22; auto key = [&](Color8 c, bool dashed, const std::string& s) { int ph = 0; seg(cv, x + 8, ly + 5, x + 30, ly + 5, c, dashed, ph); ui.text(x + 36, ly, s, TEXT); ly += 13; };
        key(E_OPT, false, "option"); key(E_COND, true, "option with a condition"); key(E_NEXT, false, "next= (auto-advance)"); key(E_BAD, false, "leads to a missing node");
        ui.text(x + 200, iy + 22, "green header: [start]", RGB(90, 200, 120)); ui.text(x + 200, iy + 35, "purple header: [camp]", RGB(170, 130, 230)); ui.text(x + 200, iy + 48, "orange: nothing leads here", ACCENT2); ui.text(x + 200, iy + 61, "red: duplicate id", DANGER);
    } else {
        auto& g = dlgG[dlgGSel];
        int ly = iy + 4;
        ui.text(x + 6, ly, "[" + g.id + "]", ACCENT2); int hx = x + 12 + font->width("[" + g.id + "]");
        if (!g.speaker.empty()) { ui.text(hx, ly, g.speaker, ACCENT); hx += font->width(g.speaker) + 8; }
        ui.text(hx, ly, fmt("line %d", g.line + 1), DIM);
        ly += 13;
        std::string full; for (auto& t : g.text) { if (!full.empty()) full += " / "; full += t; }
        auto wl = font->wrap(full.empty() ? std::string("(no text)") : full, w - 14);
        for (size_t k = 0; k < wl.size() && k < 3; k++) { std::string s = wl[k]; if (k == 2 && wl.size() > 3) s = fitText(font, s + " ...", w - 14); ui.text(x + 6, ly, s, full.empty() ? DIM : C::White); ly += 12; }
        ly += 2;
        int colW = (w - 12) / 2, rowsLeft = std::max(1, (iy + STRIP_H - 16 - ly) / 12);
        std::vector<std::pair<std::string, Color8>> items;
        for (auto& o : g.opts) items.push_back({o.label + " -> " + (o.target.empty() ? std::string("?") : o.target) + (o.cond.empty() ? "" : " if " + o.cond), (o.target != "end" && tgt(o.target) < 0) ? E_BAD : !o.cond.empty() ? E_COND : E_OPT});
        if (!g.next.empty()) items.push_back({"next -> " + g.next, tgt(g.next) < 0 && g.next != "end" ? E_BAD : E_NEXT});
        bool two = (int)items.size() > rowsLeft;
        for (size_t k = 0; k < items.size(); k++) {
            int col = two ? (int)k / rowsLeft : 0, row = two ? (int)k % rowsLeft : (int)k; if (col > 1 || row >= rowsLeft) break;
            ui.text(x + 6 + col * colW, ly + row * 12, fitText(font, items[k].first, (two ? colW : w - 12) - 6), items[k].second);
        }
        std::string acts; for (auto& a : g.acts) { if (!acts.empty()) acts += "   "; acts += a; }
        if (!acts.empty()) ui.text(x + 6, iy + STRIP_H - 16, fitText(font, "actions: " + acts, w - 12), ACCENT2);
    }
    cv.resetClip();
}
