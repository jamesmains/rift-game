// Desktop entry point. raylib (through rl_shim) owns the window and input; App draws into a 640x480 canvas.
#include "rl_shim.h"
#include "../app.h"
#include "../core/platform_services.h"
#include "../core/crash.h"
#include <filesystem>
#include <cstring>
#include <cmath>

// raylib-backed audio (see rl_shim.c)
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

int main(int argc, char** argv) {
    std::string base = rl_app_dir();
    std::string assets = base + "assets";
    std::string saves = base + "saves";
    if (!std::filesystem::exists(assets)) { assets = "assets"; saves = "saves"; } // running from the source tree
    for (int i = 1; i + 1 < argc; i++)   // --saves <folder>: keep saves, settings and desktop layout there instead (fresh test runs)
        if (std::strcmp(argv[i], "--saves") == 0) saves = argv[i + 1];
    std::error_code ec; std::filesystem::create_directories(saves, ec);

    if (!gPlatform->init()) return 0;          // Steam asked us to relaunch through the client
    rl_audio_init();
    static RlAudio audio;
    App app;
    if (!app.init(assets, saves, &audio)) {
        rl_message_box("Triffall - startup error", app.error.c_str());
        return 1;
    }
    // crash safety: a report + emergency save if the game dies, and a lock file that tells the next start it did
    bool unclean = crashLastRunUnclean(saves);
    crashSessionBegin(saves);
    if (unclean) app.desktop.offerCrashRecovery();
    int scale = app.settings.scale < 1 ? 2 : app.settings.scale;
    rl_init(UI::SCREEN_W * scale, UI::SCREEN_H * scale, "Triffall", app.settings.vsync ? 1 : 0);
    bool fullscreen = false;
    bool integer = app.settings.integerScaling;
    InputState in;
    RlFrameInput raw;
    double last = -1;

    while (true) {
        rl_poll(&raw, app.settings.gamepad ? 1 : 0);
        if (last < 0) last = raw.time;
        float dt = (float)(raw.time - last); last = raw.time;
        if (dt > 0.1f) dt = 0.1f;
        in.beginFrame();
        if (raw.shouldClose) in.quitRequested = true;
        for (int k = 0; k < K_COUNT; k++) { in.pressed[k] = raw.keyPressed[k] != 0; in.down[k] = raw.keyDown[k] != 0; in.released[k] = raw.keyReleased[k] != 0; }
        in.typed = raw.typed; in.lastKey = raw.lastKey;
        in.stickX = raw.stickX; in.stickY = raw.stickY; in.stick2X = raw.stick2X; in.stick2Y = raw.stick2Y;
        // controller glyphs stay up while the pad is in use; only a real keyboard key, a deliberate mouse move or a click
        // switches back (pad presses and auto-repeats are not keyboard keys, and a 1-pixel mouse jitter is ignored)
        bool anyKey = false; for (int k = 1; k < K_PAD_A; k++) if (raw.keyPressed[k]) anyKey = true;
        if (raw.gamepadActive) in.gamepadActive = true;
        bool mouseMoved = fabsf(raw.mouseDX) + fabsf(raw.mouseDY) >= 3.0f;
        if ((anyKey && !raw.gamepadActive) || mouseMoved || raw.mousePressed[0] || raw.mousePressed[2] || raw.wheel != 0) in.gamepadActive = false;
        // mouse → framebuffer coordinates
        int sw = raw.screenW, sh = raw.screenH, cw = app.canvas.w, chh = app.canvas.h;
        // same rule as rl_present: integer scaling only when 2x or more fits, otherwise fit the screen
        float fit = std::min((float)sw / cw, (float)sh / chh); int is = (int)fit;
        float fs = (integer && is >= 2) ? (float)is : fit; if (fs < 1.0f && integer) fs = 1.0f;
        int dw = (int)(cw * fs), dh = (int)(chh * fs);
        int ox = (sw - dw) / 2, oy = (sh - dh) / 2;
        int mx = (int)((raw.mouseX - ox) / fs), my = (int)((raw.mouseY - oy) / fs);
        // the move is measured in canvas pixels from last frame's position: dividing the raw delta by the scale threw
        // away slow movements (1 screen pixel a frame is 0 canvas pixels at 2x), so hover stuck until a faster move
        int cmx = clampi(mx, 0, cw - 1), cmy = clampi(my, 0, chh - 1);
        static int prevMX = -1, prevMY = -1;
        in.mouseDX = prevMX < 0 ? 0 : cmx - prevMX; in.mouseDY = prevMY < 0 ? 0 : cmy - prevMY; prevMX = cmx; prevMY = cmy;
        in.mouseX = cmx; in.mouseY = cmy;
        in.mouseInWindow = mx >= 0 && my >= 0 && mx < cw && my < chh && raw.cursorOnScreen;
        for (int b = 0; b < 3; b++) { in.mouseDown[b] = raw.mouseDown[b] != 0; in.mousePressed[b] = raw.mousePressed[b] != 0; in.mouseReleased[b] = raw.mouseReleased[b] != 0; }
        in.wheel = raw.wheel;
        if (in.down[K_CTRL] || in.mousePressed[0] || in.pressed[K_INSERT]) in.clipboard = rl_get_clipboard(); else in.clipboard.clear();   // 1.1.11: also for the right-click menu's Paste and Shift+Insert

        gPlatform->update();
        if (gPlatform->wantsQuit()) in.quitRequested = true;
        if (gPlatform->overlayActive()) { in.beginFrame(); dt = 0; }   // freeze input while the overlay is up
        app.frame(in, dt);
        if (app.req.hasClipboardOut) { rl_set_clipboard(app.req.setClipboard.c_str()); app.req.hasClipboardOut = false; }
        if (app.wantsQuit()) break;

        PlatformRequests& r = app.req;
        if (r.setFullscreen) { r.setFullscreen = false; if (r.fullscreenValue != fullscreen) { rl_toggle_borderless(); fullscreen = r.fullscreenValue; } if (!fullscreen) rl_set_window_size(app.canvas.w * app.settings.scale, app.canvas.h * app.settings.scale); }
        if (r.toggleFullscreen) { r.toggleFullscreen = false; rl_toggle_borderless(); fullscreen = !fullscreen; }
        if (r.setScale) { int s = r.setScale; r.setScale = 0; if (!fullscreen) rl_set_window_size(app.canvas.w * s, app.canvas.h * s); }
        if (r.setIntegerScaling) { r.setIntegerScaling = false; integer = r.integerScalingValue; }
        if (r.setVsync) { r.setVsync = false; rl_set_target_fps(r.vsyncValue ? 0 : 60); }
        if (!r.openURL.empty()) { rl_open_url(r.openURL.c_str()); r.openURL.clear(); }
        if (r.canvasW) { if (!fullscreen) { int sc = app.settings.scale; int mw = rl_monitor_w(), mh = rl_monitor_h(); while (sc > 1 && (r.canvasW * sc > mw - 80 || r.canvasH * sc > mh - 120)) sc--; rl_set_window_size(r.canvasW * sc, r.canvasH * sc); } r.canvasW = r.canvasH = 0; }

        rl_present(app.canvas.px.data(), app.canvas.w, app.canvas.h, integer ? 1 : 0, app.settings.crt ? 1 : 0);
    }
    app.settings.save(saves + "/settings.ini");
    crashSessionEnd(saves);
    gPlatform->shutdown();
    rl_shutdown();
    return 0;
}
