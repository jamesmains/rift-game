// 1.8.6 (#204): grid-like focus moves for a controller's d-pad and the arrow keys, shared by the Settings panel and the
// desktop's windows (launcher, settings, about...). The owner: "columns should go up and down, and left into another
// list should land on the most reasonable item of it".
//   - Only controls further along the pressed direction count (their centre is past the focused one's centre).
//   - Controls in the same column (Up / Down: they overlap the focused one sideways) or the same row (Left / Right)
//     always win over ones off to the side, so a column is walked in order and Left / Right lands on the item of the
//     next list that sits at the same height.
//   - Among those: the nearest edge to edge, then the one best lined up with the focused one's centre (a wide one's left
//     end, going up / down).
//   - Off to the side (nothing in line): along-distance plus three times the sideways gap.
//   - Going straight back the way you came returns to where you were (a wide slider or list row doesn't send Up to
//     some other column than the one you came down from).
#pragma once
#include "gfx.h"
#include <vector>
#include <cstdlib>
#include <algorithm>

namespace SpatNav {

const int SLACK = 16;
inline bool sameRect(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

// is r further along (dx, dy) than cur? (centre past centre)
inline bool ahead(const Rect& cur, const Rect& r, int dx, int dy) {
    int cx = cur.x + cur.w / 2, cy = cur.y + cur.h / 2, rx = r.x + r.w / 2, ry = r.y + r.h / 2;
    int along = dx ? dx * (rx - cx) : dy * (ry - cy);
    if (along <= 2) return false;
    // and its near edge is past the middle of the focused one (a wider control in the next row isn't "to the right")
    if (dx > 0) return r.x >= cx; if (dx < 0) return r.x + r.w <= cx;
    if (dy > 0) return r.y >= cy; return r.y + r.h <= cy;
}

// the control to move to from `cur` in (dx, dy) (one of them 0); `self` = cur's own index to skip; -1 = none
inline int pick(const std::vector<Rect>& rs, const Rect& cur, int dx, int dy, int self = -1) {
    int best = -1; long bestScore = 0;
    // a wide control (a slider, a list row, a row of choices) leads from its left end going up / down, the way you read
    const int cx = cur.x + std::min(cur.w, 60) / 2, cy = cur.y + cur.h / 2;
    for (int i = 0; i < (int)rs.size(); i++) {
        if (i == self) continue;
        const Rect& r = rs[i];
        if (r.w <= 0 || r.h <= 0 || sameRect(r, cur) || !ahead(cur, r, dx, dy)) continue;
        long gap, overlap, centre;
        if (dy) {
            gap = dy > 0 ? r.y - (cur.y + cur.h) : cur.y - (r.y + r.h);
            overlap = std::min(cur.x + cur.w, r.x + r.w) - std::max(cur.x, r.x);
            centre = std::abs((r.x + r.w / 2) - cx);
        } else {
            gap = dx > 0 ? r.x - (cur.x + cur.w) : cur.x - (r.x + r.w);
            overlap = std::min(cur.y + cur.h, r.y + r.h) - std::max(cur.y, r.y);
            centre = std::abs((r.y + r.h / 2) - cy);
        }
        if (gap < 0) gap = 0;
        if (dy) overlap += SLACK;   // within a few pixels of lining up counts as the same column (a row that starts just past a tab)
        long score = overlap > 0 ? gap * 4 + centre : 1000000 + gap * 2 + (-overlap) * 3 + centre / 4;
        if (best < 0 || score < bestScore) { best = i; bestScore = score; }
    }
    return best;
}

// remembers the last move, so the opposite press goes straight back
struct Memory {
    Rect from{0, 0, 0, 0}, to{0, 0, 0, 0}; int dx = 0, dy = 0; bool valid = false;
};
inline int move(const std::vector<Rect>& rs, const Rect& cur, int dx, int dy, Memory& mem, int self = -1) {
    int to = -1;
    if (mem.valid && mem.dx == -dx && mem.dy == -dy && sameRect(mem.to, cur))
        for (int i = 0; i < (int)rs.size(); i++) if (i != self && sameRect(rs[i], mem.from) && ahead(cur, rs[i], dx, dy)) { to = i; break; }
    if (to < 0) to = pick(rs, cur, dx, dy, self);
    if (to >= 0) { mem.from = cur; mem.to = rs[to]; mem.dx = dx; mem.dy = dy; mem.valid = true; }
    return to;
}

}   // namespace SpatNav
