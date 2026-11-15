#include "raylib.h"
#include "rl_shim.h"
#include <string.h>
#include <math.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
// Browsers hand over controllers raw: no dead zone, and a pad without the "standard" layout may report its triggers
// as extra axes resting at -1 (raylib then reads them as a stick held hard over) and its buttons in another order.
// So the web build only uses standard-layout pads, and measures each axis's resting value when the pad first shows up.
EM_JS(int, web_pad_standard, (), {
    var pads = navigator.getGamepads ? navigator.getGamepads() : [];
    var p = pads && pads[0];
    if (!p) return 0;
    if (!Module.triffallPadLogged || Module.triffallPadLogged !== p.id) {
        Module.triffallPadLogged = p.id;
        console.log('Triffall: controller "' + p.id + '", layout "' + (p.mapping || 'none') + '", ' + p.axes.length + ' axes, ' + p.buttons.length + ' buttons' +
                    (p.mapping === 'standard' ? '' : ' - not a standard layout, so it is ignored in the browser'));
    }
    return p.mapping === 'standard' ? 1 : 0;
});
static float gAxisRest[4]; static int gAxisRestSet = 0;
static float padAxis(int axis) {
    float v = GetGamepadAxisMovement(0, axis) - (gAxisRestSet ? gAxisRest[axis] : 0.0f);
    return fabsf(v) < 0.30f ? 0.0f : v;   // a wider dead zone than the desktop's: browsers apply none of their own
}
#else
static float padAxis(int axis) { return GetGamepadAxisMovement(0, axis); }
#endif

#if defined(_WIN32)
int __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType);
#endif

// Our Key enum order (must match input.h)
enum { K_NONE = 0, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESCAPE, K_SPACE, K_TAB, K_BACKSPACE, K_DELETE,
       K_PAGEUP, K_PAGEDOWN, K_HOME, K_END, K_INSERT, K_SHIFT, K_CTRL, K_ALT,
       K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
       K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
       K_A, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M, K_N, K_O, K_P, K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,
       K_MINUS, K_EQUALS, K_LBRACKET, K_RBRACKET, K_COMMA, K_PERIOD, K_SLASH, K_GRAVE,
       K_PAD_A, K_PAD_B, K_PAD_X, K_PAD_Y, K_PAD_LB, K_PAD_RB, K_PAD_LT, K_PAD_RT,
       K_PAD_START, K_PAD_BACK, K_PAD_UP, K_PAD_DOWN, K_PAD_LEFT, K_PAD_RIGHT, K_PAD_LS, K_PAD_RS, K_COUNT };

static const int kRayKeys[K_COUNT] = {
    0, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESCAPE, KEY_SPACE, KEY_TAB, KEY_BACKSPACE, KEY_DELETE,
    KEY_PAGE_UP, KEY_PAGE_DOWN, KEY_HOME, KEY_END, KEY_INSERT, KEY_LEFT_SHIFT, KEY_LEFT_CONTROL, KEY_LEFT_ALT,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_MINUS, KEY_EQUAL, KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET, KEY_COMMA, KEY_PERIOD, KEY_SLASH, KEY_GRAVE,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const int kPadButtons[16] = {
    GAMEPAD_BUTTON_RIGHT_FACE_DOWN, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, GAMEPAD_BUTTON_RIGHT_FACE_LEFT, GAMEPAD_BUTTON_RIGHT_FACE_UP,
    GAMEPAD_BUTTON_LEFT_TRIGGER_1, GAMEPAD_BUTTON_RIGHT_TRIGGER_1, GAMEPAD_BUTTON_LEFT_TRIGGER_2, GAMEPAD_BUTTON_RIGHT_TRIGGER_2,
    GAMEPAD_BUTTON_MIDDLE_RIGHT, GAMEPAD_BUTTON_MIDDLE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_UP, GAMEPAD_BUTTON_LEFT_FACE_DOWN,
    GAMEPAD_BUTTON_LEFT_FACE_LEFT, GAMEPAD_BUTTON_LEFT_FACE_RIGHT, GAMEPAD_BUTTON_LEFT_THUMB, GAMEPAD_BUTTON_RIGHT_THUMB
};

static Texture2D gTex;
static int gTexW = 0, gTexH = 0;
static RenderTexture2D gRT;            // 1.0: the frame upscaled by an integer, then smoothed down to fit (small screens)
static int gRTW = 0, gRTH = 0;
static int gLastDX = 0, gLastDY = 0;
static Shader gCrtShader;
static int gCrtReady = 0, gCrtResolutionLoc = -1;

static const char* gCrtFragmentShader =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 p = fragTexCoord * 2.0 - 1.0;\n"
    "    p *= 1.0 + 0.018 * dot(p, p);\n"
    "    vec2 uv = p * 0.5 + 0.5;\n"
    "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { finalColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "    vec2 rgbShift = vec2(0.0008, 0.0);\n"
    "    float red = texture(texture0, uv + rgbShift).r;\n"
    "    float green = texture(texture0, uv).g;\n"
    "    float blue = texture(texture0, uv - rgbShift).b;\n"
    "    float scanline = 0.92 + 0.08 * cos(uv.y * resolution.y * 3.14159265);\n"
    "    float vignette = clamp(1.0 - 0.12 * dot(p, p), 0.72, 1.0);\n"
    "    finalColor = vec4(vec3(red, green, blue) * scanline * vignette * fragColor.rgb, 1.0);\n"
    "}\n";

#if defined(__EMSCRIPTEN__)
// WebGL 1 takes GLSL ES 1.00: the same CRT effect in that dialect
static const char* gCrtFragmentShaderES =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "    vec2 p = fragTexCoord * 2.0 - 1.0;\n"
    "    p *= 1.0 + 0.018 * dot(p, p);\n"
    "    vec2 uv = p * 0.5 + 0.5;\n"
    "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "    vec2 rgbShift = vec2(0.0008, 0.0);\n"
    "    float red = texture2D(texture0, uv + rgbShift).r;\n"
    "    float green = texture2D(texture0, uv).g;\n"
    "    float blue = texture2D(texture0, uv - rgbShift).b;\n"
    "    float scanline = 0.92 + 0.08 * cos(uv.y * resolution.y * 3.14159265);\n"
    "    float vignette = clamp(1.0 - 0.12 * dot(p, p), 0.72, 1.0);\n"
    "    gl_FragColor = vec4(vec3(red, green, blue) * scanline * vignette * fragColor.rgb, 1.0);\n"
    "}\n";
#define CRT_SHADER_SOURCE gCrtFragmentShaderES
#else
#define CRT_SHADER_SOURCE gCrtFragmentShader
#endif

void rl_init(int w, int h, const char* title, int vsync) {
    SetTraceLogLevel(LOG_WARNING);
#if defined(__EMSCRIPTEN__)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | (vsync ? FLAG_VSYNC_HINT : 0));
#else
    // 1.2.1: start hidden; the caller sizes the window to fit the monitor (rl_place_window) and then shows it
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIDDEN | (vsync ? FLAG_VSYNC_HINT : 0));
#endif
    InitWindow(w, h, title);
    SetExitKey(KEY_NULL);
    SetTargetFPS(vsync ? 0 : 60);
#if defined(__EMSCRIPTEN__)
    // the page's CSS hides the pointer; raylib's web HideCursor() would also stop it tracking the mouse position
    // (it treats a hidden cursor as pointer-locked), which left the game's cursor stuck at the top-left corner
#else
    HideCursor();
#endif
    gCrtShader = LoadShaderFromMemory(NULL, CRT_SHADER_SOURCE);
    gCrtReady = gCrtShader.id > 0;
    if (gCrtReady) gCrtResolutionLoc = GetShaderLocation(gCrtShader, "resolution");
    else TraceLog(LOG_WARNING, "CRT shader unavailable; using standard presentation");
}

void rl_shutdown(void) {
    if (gTexW) UnloadTexture(gTex);
    if (gRTW) UnloadRenderTexture(gRT);
    if (gCrtReady) UnloadShader(gCrtShader);
    rl_audio_close();
    CloseWindow();
}

void rl_poll(RlFrameInput* in, int gamepadEnabled) {
    memset(in, 0, sizeof(*in));
#if defined(__EMSCRIPTEN__)
    in->shouldClose = 0;   // a page has no window to close, and raylib's web WindowShouldClose() sleeps (needs Asyncify)
#else
    in->shouldClose = WindowShouldClose();
#endif
    in->lastKey = 0;
    for (int k = 1; k < K_PAD_A; k++) {
        int rk = kRayKeys[k]; if (!rk) continue;
        if (IsKeyPressed(rk) || IsKeyPressedRepeat(rk)) { in->keyPressed[k] = 1; if (IsKeyPressed(rk)) in->lastKey = k; }
        if (IsKeyDown(rk)) in->keyDown[k] = 1;
        if (IsKeyReleased(rk)) in->keyReleased[k] = 1;
    }
    // right-side modifiers map onto the same virtual key
    if (IsKeyDown(KEY_RIGHT_SHIFT)) in->keyDown[K_SHIFT] = 1;
    if (IsKeyDown(KEY_RIGHT_CONTROL)) in->keyDown[K_CTRL] = 1;
    if (IsKeyDown(KEY_RIGHT_ALT)) in->keyDown[K_ALT] = 1;
    if (IsKeyPressed(KEY_KP_ENTER)) { in->keyPressed[K_ENTER] = 1; in->lastKey = K_ENTER; }
    if (IsKeyDown(KEY_KP_ENTER)) in->keyDown[K_ENTER] = 1;
    int n = 0, ch;
    while ((ch = GetCharPressed()) > 0) { if (ch >= 32 && ch < 127 && n < 31) in->typed[n++] = (char)ch; }
    in->typed[n] = 0;
#if defined(__EMSCRIPTEN__)
    if (gamepadEnabled && IsGamepadAvailable(0) && !web_pad_standard()) gamepadEnabled = 0;
    if (gamepadEnabled && IsGamepadAvailable(0) && !gAxisRestSet) {
        for (int a = 0; a < 4; a++) { float r = GetGamepadAxisMovement(0, a); gAxisRest[a] = fabsf(r) < 0.5f ? r : 0.0f; }
        gAxisRestSet = 1;
    }
#endif
    if (gamepadEnabled && IsGamepadAvailable(0)) {
        for (int b = 0; b < 16; b++) {
            int k = K_PAD_A + b;
            if (IsGamepadButtonPressed(0, kPadButtons[b])) { in->keyPressed[k] = 1; in->gamepadActive = 1; in->lastKey = k; }
            if (IsGamepadButtonDown(0, kPadButtons[b])) { in->keyDown[k] = 1; in->gamepadActive = 1; }
            if (IsGamepadButtonReleased(0, kPadButtons[b])) in->keyReleased[k] = 1;
        }
        float sx = padAxis(GAMEPAD_AXIS_LEFT_X), sy = padAxis(GAMEPAD_AXIS_LEFT_Y);
        float rx = padAxis(GAMEPAD_AXIS_RIGHT_X), ry = padAxis(GAMEPAD_AXIS_RIGHT_Y);
        if (fabsf(sx) > 0.25f || fabsf(sy) > 0.25f) { in->stickX = sx; in->stickY = sy; in->gamepadActive = 1; }
        if (fabsf(rx) > 0.25f || fabsf(ry) > 0.25f) { in->stick2X = rx; in->stick2Y = ry; in->gamepadActive = 1; }
        int dx = sx > 0.5f ? 1 : sx < -0.5f ? -1 : 0, dy = sy > 0.5f ? 1 : sy < -0.5f ? -1 : 0;
        if (dx != gLastDX && dx) in->keyPressed[dx > 0 ? K_PAD_RIGHT : K_PAD_LEFT] = 1;
        if (dy != gLastDY && dy) in->keyPressed[dy > 0 ? K_PAD_DOWN : K_PAD_UP] = 1;
        if (dx) in->keyDown[dx > 0 ? K_PAD_RIGHT : K_PAD_LEFT] = 1;
        if (dy) in->keyDown[dy > 0 ? K_PAD_DOWN : K_PAD_UP] = 1;
        gLastDX = dx; gLastDY = dy;
        // held D-pad / stick directions repeat like a held keyboard arrow (lists scroll, quantities run)
        static double holdStart[4] = {0, 0, 0, 0}, lastRep[4] = {0, 0, 0, 0};
        double now = GetTime();
        for (int d = 0; d < 4; d++) {
            int k = K_PAD_UP + d;
            if (!in->keyDown[k]) { holdStart[d] = 0; continue; }
            if (in->keyPressed[k] || holdStart[d] == 0) { holdStart[d] = now; lastRep[d] = now; continue; }
            if (now - holdStart[d] > 0.40 && now - lastRep[d] > 0.07) { in->keyPressed[k] = 1; in->gamepadActive = 1; lastRep[d] = now; }
        }
    }
    Vector2 mp = GetMousePosition(), md = GetMouseDelta();
    in->mouseX = mp.x; in->mouseY = mp.y; in->mouseDX = md.x; in->mouseDY = md.y;
    in->mouseDown[0] = IsMouseButtonDown(MOUSE_BUTTON_LEFT); in->mousePressed[0] = IsMouseButtonPressed(MOUSE_BUTTON_LEFT); in->mouseReleased[0] = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    in->mouseDown[1] = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE); in->mousePressed[1] = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE); in->mouseReleased[1] = IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE);
    in->mouseDown[2] = IsMouseButtonDown(MOUSE_BUTTON_RIGHT); in->mousePressed[2] = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT); in->mouseReleased[2] = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
    in->wheel = (int)GetMouseWheelMove();
    in->cursorOnScreen = IsCursorOnScreen();
    in->screenW = GetScreenWidth(); in->screenH = GetScreenHeight();
    in->time = GetTime();
}

void rl_present(const void* rgba, int w, int h, int integerScaling, int crt) {
    if (gTexW != w || gTexH != h) {
        if (gTexW) UnloadTexture(gTex);
        Image img = GenImageColor(w, h, BLACK);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        gTex = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(gTex, TEXTURE_FILTER_POINT);
        gTexW = w; gTexH = h;
    }
    UpdateTexture(gTex, rgba);
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // Scaling: integer when the screen fits at least 2x (crisp pixels). Below that - a Steam Deck's 1280x800 only fits
    // 1x of a 640x480 frame, which leaves tiny text - the frame is drawn at 2x (or 3x) with sharp pixels and then scaled
    // down smoothly to fill the screen, so it stays readable without the uneven pixels of a plain fractional scale.
    float a = (float)sw / w, b = (float)sh / h, fit = a < b ? a : b;
    int is = (int)fit;
    int useInt = integerScaling && is >= 2;
    float fs = useInt ? (float)is : fit;
    if (fs < 1.0f && integerScaling) fs = 1.0f;
    int dw = (int)(w * fs), dh = (int)(h * fs);
    int ox = (sw - dw) / 2, oy = (sh - dh) / 2;
    int smooth = !useInt && fit > 1.01f;
    if (smooth) {
        int k = (int)ceilf(fit); if (k < 2) k = 2;
        if (gRTW != w * k || gRTH != h * k) { if (gRTW) UnloadRenderTexture(gRT); gRT = LoadRenderTexture(w * k, h * k); SetTextureFilter(gRT.texture, TEXTURE_FILTER_BILINEAR); gRTW = w * k; gRTH = h * k; }
        BeginTextureMode(gRT);
        ClearBackground(BLACK);
        DrawTexturePro(gTex, (Rectangle){0, 0, (float)w, (float)h}, (Rectangle){0, 0, (float)gRTW, (float)gRTH}, (Vector2){0, 0}, 0, WHITE);
        EndTextureMode();
    }
    BeginDrawing();
    ClearBackground(BLACK);
    if (crt && gCrtReady) {
        float resolution[2] = {(float)w, (float)h};
        if (gCrtResolutionLoc >= 0) SetShaderValue(gCrtShader, gCrtResolutionLoc, resolution, SHADER_UNIFORM_VEC2);
        BeginShaderMode(gCrtShader);
    }
    if (smooth) DrawTexturePro(gRT.texture, (Rectangle){0, 0, (float)gRTW, -(float)gRTH}, (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh}, (Vector2){0, 0}, 0, WHITE);
    else DrawTexturePro(gTex, (Rectangle){0, 0, (float)w, (float)h}, (Rectangle){(float)ox, (float)oy, (float)dw, (float)dh}, (Vector2){0, 0}, 0, WHITE);
    if (crt && gCrtReady) EndShaderMode();
    EndDrawing();
}

// raylib marks a borderless window "floating" (always on top), which hides Alt+Tab targets behind the game; undo that.
void rl_toggle_borderless(void) { ToggleBorderlessWindowed(); if (IsWindowState(FLAG_WINDOW_TOPMOST)) ClearWindowState(FLAG_WINDOW_TOPMOST); }
void rl_set_window_size(int w, int h) { SetWindowSize(w, h); }
// 1.2.1: size the window and keep it on its monitor: centre it when asked, or when any part (or the title bar)
// would be off the monitor. The position raylib reports is the client area's; the title bar is ~31 px above it.
void rl_place_window(int w, int h, int center) {
    SetWindowSize(w, h);
#if !defined(__EMSCRIPTEN__)
    int m = GetCurrentMonitor(); Vector2 mp = GetMonitorPosition(m);
    int mx = (int)mp.x, my = (int)mp.y, mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
    if (mw <= 0 || mh <= 0) return;
    Vector2 wp = GetWindowPosition(); int x = (int)wp.x, y = (int)wp.y;
    const int TITLE = 32, BOTTOM = 48;   // title bar above the client area; taskbar at the bottom
    if (center || x < mx || y < my + TITLE || x + w > mx + mw || y + h > my + mh - BOTTOM) {
        x = mx + (mw - w) / 2; y = my + TITLE + (mh - TITLE - BOTTOM - h) / 2;
        if (x < mx) x = mx;
        if (y < my + TITLE) y = my + TITLE;
        SetWindowPosition(x, y);
    }
#else
    (void)center;
#endif
}
void rl_show_window(void) {
#if !defined(__EMSCRIPTEN__)
    if (IsWindowState(FLAG_WINDOW_HIDDEN)) ClearWindowState(FLAG_WINDOW_HIDDEN);
#endif
}
void rl_set_target_fps(int fps) { SetTargetFPS(fps); }
void rl_open_url(const char* url) { OpenURL(url); }
const char* rl_app_dir(void) { return GetApplicationDirectory(); }
const char* rl_get_clipboard(void) { const char* c = GetClipboardText(); return c ? c : ""; }
void rl_set_clipboard(const char* s) { SetClipboardText(s); }
// 1.2.0: files dropped on the window this frame, one path per line ("" when none)
const char* rl_dropped_files(void) {
    static char buf[8192]; buf[0] = 0;
    if (!IsFileDropped()) return buf;
    FilePathList l = LoadDroppedFiles(); size_t n = 0;
    for (unsigned int i = 0; i < l.count; i++) { size_t k = strlen(l.paths[i]); if (n + k + 2 >= sizeof(buf)) break; if (n) buf[n++] = 10; memcpy(buf + n, l.paths[i], k); n += k; buf[n] = 0; }
    UnloadDroppedFiles(l);
    return buf;
}
int rl_monitor_w(void) { return GetMonitorWidth(GetCurrentMonitor()); }
int rl_monitor_h(void) { return GetMonitorHeight(GetCurrentMonitor()); }
void rl_message_box(const char* title, const char* text) {
#if defined(_WIN32)
    MessageBoxA(0, text, title, 0x10);
#else
    TraceLog(LOG_ERROR, "%s: %s", title, text);
#endif
}

// ---------------- audio ----------------
#define RL_MAX_SOUNDS 1024   /* 1.3.4: was 256, and the base game already loaded 252 files */
#define RL_ALIASES 4
static Sound gSounds[RL_MAX_SOUNDS][RL_ALIASES];
static int gSoundCount = 0, gAudioReady = 0;
static Music gMusic; static int gMusicLoaded = 0;
void rl_audio_init(void) { if (!gAudioReady) { InitAudioDevice(); gAudioReady = IsAudioDeviceReady(); } }
void rl_audio_close(void) { if (!gAudioReady) return; rl_ambient_stop(); rl_audio_unload_all(); if (gMusicLoaded) { StopMusicStream(gMusic); UnloadMusicStream(gMusic); gMusicLoaded = 0; } CloseAudioDevice(); gAudioReady = 0; }
static int rl_sound_slot(Sound s) {   /* a free slot (left by rl_sound_unload) or a new one */
    int id = -1;
    for (int i = 0; i < gSoundCount; i++) if (gSounds[i][0].frameCount == 0) { id = i; break; }
    if (id < 0) { if (gSoundCount >= RL_MAX_SOUNDS) { UnloadSound(s); return -1; } id = gSoundCount++; }
    gSounds[id][0] = s; for (int i = 1; i < RL_ALIASES; i++) gSounds[id][i] = LoadSoundAlias(s);
    return id;
}
int rl_sound_load(const char* path) {
    if (!gAudioReady) return -1;
    Sound s = LoadSound(path); if (s.frameCount == 0) return -1;
    return rl_sound_slot(s);
}
/* 1.3.4 (#52): the file as 32-bit float samples (interleaved) for the game to process, then made into a sound */
float* rl_wave_load_f32(const char* path, int* frames, int* channels, int* rate) {
    Wave w = LoadWave(path); if (w.frameCount == 0 || !w.data) return 0;
    WaveFormat(&w, w.sampleRate, 32, w.channels);
    size_t n = (size_t)w.frameCount * w.channels;
    float* out = (float*)MemAlloc((unsigned int)(n * sizeof(float)));
    if (out) memcpy(out, w.data, n * sizeof(float));
    *frames = (int)w.frameCount; *channels = (int)w.channels; *rate = (int)w.sampleRate;
    UnloadWave(w);
    return out;
}
void rl_wave_free(float* data) { if (data) MemFree(data); }
int rl_sound_from_f32(const float* data, int frames, int channels, int rate) {
    if (!gAudioReady || !data || frames <= 0) return -1;
    Wave w; w.frameCount = (unsigned int)frames; w.sampleRate = (unsigned int)rate; w.sampleSize = 32; w.channels = (unsigned int)channels; w.data = (void*)data;
    Sound s = LoadSoundFromWave(w); if (s.frameCount == 0) return -1;
    return rl_sound_slot(s);
}
void rl_sound_unload(int id) {
    if (!gAudioReady || id < 0 || id >= gSoundCount || gSounds[id][0].frameCount == 0) return;
    for (int a = 0; a < RL_ALIASES; a++) StopSound(gSounds[id][a]);
    for (int a = 1; a < RL_ALIASES; a++) UnloadSoundAlias(gSounds[id][a]);
    UnloadSound(gSounds[id][0]);
    for (int a = 0; a < RL_ALIASES; a++) gSounds[id][a].frameCount = 0;
}
void rl_sound_play(int id, float vol, float pitch) {
    if (!gAudioReady || id < 0 || id >= gSoundCount || gSounds[id][0].frameCount == 0) return;
    static int rr[RL_MAX_SOUNDS];
    int a = rr[id] = (rr[id] + 1) % RL_ALIASES;       // round-robin aliases so quick repeats overlap instead of cutting off
    SetSoundVolume(gSounds[id][a], vol); SetSoundPitch(gSounds[id][a], pitch); PlaySound(gSounds[id][a]);
}
void rl_audio_unload_all(void) {
    if (!gAudioReady) return;
    for (int i = 0; i < gSoundCount; i++) { if (gSounds[i][0].frameCount == 0) continue; for (int a = 1; a < RL_ALIASES; a++) UnloadSoundAlias(gSounds[i][a]); UnloadSound(gSounds[i][0]); }
    gSoundCount = 0;
}
int rl_music_play(const char* path, float vol, int loop) {
    if (!gAudioReady) return 0;
    if (gMusicLoaded) { StopMusicStream(gMusic); UnloadMusicStream(gMusic); gMusicLoaded = 0; }
    gMusic = LoadMusicStream(path); if (gMusic.frameCount == 0) return 0;
    gMusic.looping = loop != 0; gMusicLoaded = 1;
    SetMusicVolume(gMusic, vol); PlayMusicStream(gMusic);
    return 1;
}
void rl_music_stop(void) { if (gMusicLoaded) { StopMusicStream(gMusic); UnloadMusicStream(gMusic); gMusicLoaded = 0; } }
void rl_music_volume(float vol) { if (gMusicLoaded) SetMusicVolume(gMusic, vol); }
int rl_music_playing(void) { return gMusicLoaded && IsMusicStreamPlaying(gMusic); }
static Music gAmb; static int gAmbLoaded = 0;
int rl_ambient_play(const char* path, float vol) {
    if (!gAudioReady) return 0;
    if (gAmbLoaded) { StopMusicStream(gAmb); UnloadMusicStream(gAmb); gAmbLoaded = 0; }
    gAmb = LoadMusicStream(path); if (gAmb.frameCount == 0) return 0;
    gAmb.looping = true; gAmbLoaded = 1; SetMusicVolume(gAmb, vol); PlayMusicStream(gAmb); return 1;
}
void rl_ambient_stop(void) { if (gAmbLoaded) { StopMusicStream(gAmb); UnloadMusicStream(gAmb); gAmbLoaded = 0; } }
void rl_ambient_volume(float vol) { if (gAmbLoaded) SetMusicVolume(gAmb, vol); }
void rl_music_update(void) { if (gMusicLoaded) UpdateMusicStream(gMusic); if (gAmbLoaded) UpdateMusicStream(gAmb); }
