// Browser entry point (cmake with emcmake, see README "Browser build"). The same App, game and raylib shim as the
// desktop build in main_raylib.cpp; only what a browser does differently lives here:
//   * the browser calls web_frame() once per frame instead of the game owning a while loop;
//   * saves/ lives in the browser's IndexedDB and is copied there shortly after anything in it changes;
//   * no crash handler, no window sizing (the canvas fills the page), fullscreen is the browser's;
//   * Shut Down shows a "safe to turn off" screen instead of closing a window.
// Nothing in the game code knows it is running in a browser.
#include <emscripten.h>
#include "rl_shim.h"
#include "../app.h"
#include "../core/platform_services.h"
#include <cmath>
#include <algorithm>

struct RlAudio : AudioBackend {
    int loadSound(const std::string& path) override { return rl_sound_load(path.c_str()); }
    void playSound(int h, float vol, float pitch) override { rl_sound_play(h, vol, pitch); }
    bool playMusic(const std::string& path, float vol, bool loop) override { return rl_music_play(path.c_str(), vol, loop ? 1 : 0) != 0; }
    void stopMusic() override { rl_music_stop(); }
    void setMusicVolume(float v) override { rl_music_volume(v); }
    bool musicPlaying() override { return rl_music_playing() != 0; }
    void update() override { rl_music_update(); }
    void unloadAll() override { rl_audio_unload_all(); }
    bool playAmbient(const std::string& path, float vol) override { return rl_ambient_play(path.c_str(), vol) != 0; }
    void stopAmbient() override { rl_ambient_stop(); }
    void setAmbientVolume(float v) override { rl_ambient_volume(v); }
};

// ---- browser storage for saves/ ----
// The game writes files in /saves as usual. Every few frames this compares the folder's file list, sizes and times
// with the last copy and, when anything changed, copies the folder to IndexedDB (one copy at a time).
EM_JS(void, web_saves_persist, (), {
    var st = Module.triffallSaves || (Module.triffallSaves = { sig: '', busy: false, again: false });
    if (st.busy) { st.again = true; return; }
    st.busy = true;
    FS.syncfs(false, function(err) {
        st.busy = false;
        if (err) console.warn('Triffall: could not store saves in the browser', err);
        if (st.again) { st.again = false; _web_saves_persist_again(); }
    });
});
EM_JS(int, web_saves_changed, (), {
    var st = Module.triffallSaves || (Module.triffallSaves = { sig: '', busy: false, again: false });
    var sig = '';
    function walk(p) {
        var names = FS.readdir(p);
        for (var i = 0; i < names.length; i++) {
            var n = names[i]; if (n === '.' || n === '..') continue;
            var q = p + '/' + n, s = FS.stat(q);
            if (FS.isDir(s.mode)) walk(q); else sig += q + ':' + s.size + ':' + (+s.mtime) + ';';
        }
    }
    try { walk('/saves'); } catch (e) { return 0; }
    if (sig === st.sig) return 0;
    st.sig = sig; return 1;
});
extern "C" EMSCRIPTEN_KEEPALIVE void web_saves_persist_again() { web_saves_persist(); }

EM_JS(int, web_canvas_w, (), { return Module.canvas.clientWidth | 0; });
EM_JS(int, web_canvas_h, (), { return Module.canvas.clientHeight | 0; });
EM_JS(void, web_set_fullscreen, (int on), {
    // a page may only go fullscreen straight after a click or key press: the "start fullscreen" setting applied at boot
    // is skipped (the player can press F11 or use Settings), and a refusal is never an error
    var el = document.getElementById('stage') || Module.canvas;
    try {
        if (on && !document.fullscreenElement && el.requestFullscreen) {
            if (navigator.userActivation && !navigator.userActivation.isActive) return;
            var p = el.requestFullscreen(); if (p && p.catch) p.catch(function() {});
        } else if (!on && document.fullscreenElement) { var q = document.exitFullscreen(); if (q && q.catch) q.catch(function() {}); }
    } catch (e) {}
});
// whether the pointer is over the game: the page tracks it (the canvas sees no "enter" when the pointer was already
// over it as the start screen went away, so raylib's own flag would stay false and the game would hide its cursor)
EM_JS(int, web_pointer_inside, (), { return Module.triffallPointerInside === false ? 0 : 1; });
EM_JS(int, web_is_fullscreen, (), { return document.fullscreenElement ? 1 : 0; });
EM_JS(void, web_show_shutdown, (), { if (Module.triffallShutdown) Module.triffallShutdown(); });
EM_JS(void, web_show_error, (const char* msg), { var m = UTF8ToString(msg); console.error(m); if (Module.triffallError) Module.triffallError(m); });

static RlAudio gAudioBackend;
static App* gApp = nullptr;
static InputState gIn;
static RlFrameInput gRaw;
static double gLast = -1;
static int gFrameNo = 0;
static int gPrevMX = -1, gPrevMY = -1;
static const char* SAVES = "/saves";

static void webFrame() {
    App& app = *gApp; InputState& in = gIn; RlFrameInput& raw = gRaw;
    // the canvas fills the page (or the itch.io frame): keep the drawing buffer the size it is shown at
    int cw0 = web_canvas_w(), ch0 = web_canvas_h();
    if (cw0 > 0 && ch0 > 0 && (cw0 != gRaw.screenW || ch0 != gRaw.screenH)) rl_set_window_size(cw0, ch0);   // gRaw: last frame's size

    rl_poll(&raw, app.settings.gamepad ? 1 : 0);
    if (gLast < 0) gLast = raw.time;
    float dt = (float)(raw.time - gLast); gLast = raw.time;
    if (dt > 0.1f) dt = 0.1f;
    in.beginFrame();
    for (int k = 0; k < K_COUNT; k++) { in.pressed[k] = raw.keyPressed[k] != 0; in.down[k] = raw.keyDown[k] != 0; in.released[k] = raw.keyReleased[k] != 0; }
    in.typed = raw.typed; in.lastKey = raw.lastKey;
    // a browser keeps Esc for itself in fullscreen (it leaves fullscreen and the page never sees the key), so in the
    // browser Backspace works as Esc too - except while a text field is taking typing (the hero name, the console)
    {
        bool typing = app.game.mode == GMode::CONSOLE ||
            (app.state == AppState::DESKTOP && !app.desktop.windows.empty() && app.desktop.windows.back().type == "new" && (app.desktop.windows.back().sub == 0 || app.desktop.windows.back().sub == 2));
        if (!typing) {
            if (raw.keyPressed[K_BACKSPACE]) { in.pressed[K_ESCAPE] = true; in.pressed[K_BACKSPACE] = false; if (in.lastKey == K_BACKSPACE) in.lastKey = K_ESCAPE; }
            if (raw.keyDown[K_BACKSPACE]) in.down[K_ESCAPE] = true;
            if (raw.keyReleased[K_BACKSPACE]) in.released[K_ESCAPE] = true;
        }
    }
    in.stickX = raw.stickX; in.stickY = raw.stickY; in.stick2X = raw.stick2X; in.stick2Y = raw.stick2Y;
    // who is driving: the mouse wins whenever it moves or clicks, and for a second afterwards a controller only takes
    // over with a button press (a stick that never quite rests must not lock the mouse out)
    bool anyKey = false; for (int k = 1; k < K_PAD_A; k++) if (raw.keyPressed[k]) anyKey = true;
    bool padButton = false; for (int k = K_PAD_A; k <= K_PAD_BACK; k++) if (raw.keyPressed[k]) padButton = true;
    if (raw.keyPressed[K_PAD_LS] || raw.keyPressed[K_PAD_RS]) padButton = true;
    bool mouseUsed = fabsf(raw.mouseDX) + fabsf(raw.mouseDY) >= 2.0f || raw.mousePressed[0] || raw.mousePressed[1] || raw.mousePressed[2] || raw.wheel != 0;
    static float mouseRecent = 0; if (mouseUsed) mouseRecent = 1.0f; else mouseRecent = std::max(0.0f, mouseRecent - dt);
    if (padButton || (raw.gamepadActive && mouseRecent <= 0)) in.gamepadActive = true;
    if ((anyKey && !raw.gamepadActive) || mouseUsed) in.gamepadActive = false;
    if (mouseRecent > 0 && !padButton) { in.stickX = in.stickY = in.stick2X = in.stick2Y = 0; }
    // mouse -> framebuffer coordinates (the same rule as rl_present and the desktop build)
    int sw = raw.screenW, sh = raw.screenH, cw = app.canvas.w, chh = app.canvas.h;
    bool integer = app.settings.integerScaling;
    float fit = std::min((float)sw / cw, (float)sh / chh); int is = (int)fit;
    float fs = (integer && is >= 2) ? (float)is : fit; if (fs < 1.0f && integer) fs = 1.0f;
    int dw = (int)(cw * fs), dh = (int)(chh * fs);
    int ox = (sw - dw) / 2, oy = (sh - dh) / 2;
    int mx = (int)((raw.mouseX - ox) / fs), my = (int)((raw.mouseY - oy) / fs);
    int cmx = clampi(mx, 0, cw - 1), cmy = clampi(my, 0, chh - 1);
    in.mouseDX = gPrevMX < 0 ? 0 : cmx - gPrevMX; in.mouseDY = gPrevMY < 0 ? 0 : cmy - gPrevMY; gPrevMX = cmx; gPrevMY = cmy;
    in.mouseX = cmx; in.mouseY = cmy;
    in.mouseInWindow = mx >= 0 && my >= 0 && mx < cw && my < chh && web_pointer_inside();
    for (int b = 0; b < 3; b++) { in.mouseDown[b] = raw.mouseDown[b] != 0; in.mousePressed[b] = raw.mousePressed[b] != 0; in.mouseReleased[b] = raw.mouseReleased[b] != 0; }
    in.wheel = raw.wheel;
    in.clipboard.clear();   // a browser only hands over the clipboard on a paste event

    app.frame(in, dt);
    app.req.hasClipboardOut = false;

    PlatformRequests& r = app.req;
    if (r.setFullscreen) { r.setFullscreen = false; web_set_fullscreen(r.fullscreenValue ? 1 : 0); }
    if (r.toggleFullscreen) { r.toggleFullscreen = false; web_set_fullscreen(web_is_fullscreen() ? 0 : 1); }
    r.setScale = 0; r.setIntegerScaling = false; r.setVsync = false; r.canvasW = r.canvasH = 0;   // the page decides the size
    if (!r.openURL.empty()) { rl_open_url(r.openURL.c_str()); r.openURL.clear(); }

    if (app.wantsQuit()) {
        app.settings.save(std::string(SAVES) + "/settings.ini");
        if (web_saves_changed()) web_saves_persist();
        emscripten_cancel_main_loop();
        web_show_shutdown();
        return;
    }
    rl_present(app.canvas.px.data(), app.canvas.w, app.canvas.h, integer ? 1 : 0, app.settings.crt ? 1 : 0);
    if (++gFrameNo % 20 == 0 && web_saves_changed()) web_saves_persist();
}

// Called by the page once the saves are loaded from IndexedDB and the player has clicked (so sound may start).
extern "C" EMSCRIPTEN_KEEPALIVE void web_start() {
    if (gApp) return;
    rl_audio_init();
    gApp = new App();
    if (!gApp->init("/assets", SAVES, &gAudioBackend)) { web_show_error(("startup error: " + gApp->error).c_str()); return; }
    web_saves_changed();   // remember the folder as loaded, so the first copy happens only after a real change
    int w = web_canvas_w(), h = web_canvas_h(); if (w <= 0 || h <= 0) { w = UI::SCREEN_W * 2; h = UI::SCREEN_H * 2; }
    rl_init(w, h, "Triffall", 1);
    emscripten_set_main_loop(webFrame, 0, 0);
}

int main() {
    gPlatform->init();
    // saves/ is a folder kept in the browser's IndexedDB; the page starts the game once it has been read back
    EM_ASM({
        FS.mkdir('/saves');
        FS.mount(IDBFS, {}, '/saves');
        FS.syncfs(true, function(err) {
            if (err) console.warn('Triffall: could not read saves from the browser', err);
            Module.triffallReady = true;
            if (Module.onTriffallReady) Module.onTriffallReady();
        });
        window.addEventListener('pagehide', function() { try { FS.syncfs(false, function() {}); } catch (e) {} });
        document.addEventListener('visibilitychange', function() { if (document.visibilityState === 'hidden') { try { FS.syncfs(false, function() {}); } catch (e) {} } });
    });
    return 0;
}
