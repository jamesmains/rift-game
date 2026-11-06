// Thin C wrapper around raylib so the C++ game code never sees raylib's headers (its Font/Color/Image names clash).
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RlFrameInput {
    int keyPressed[128];  // indexed by our Key enum
    int keyDown[128];
    int keyReleased[128];
    char typed[32];
    float stickX, stickY, stick2X, stick2Y;
    int gamepadActive;
    int lastKey;
    float mouseX, mouseY, mouseDX, mouseDY; // screen pixels
    int mouseDown[3], mousePressed[3], mouseReleased[3];
    int wheel;
    int cursorOnScreen;
    int shouldClose;
    int screenW, screenH;
    double time;
} RlFrameInput;

void rl_init(int w, int h, const char* title, int vsync);
void rl_shutdown(void);
void rl_poll(RlFrameInput* out, int gamepadEnabled);
void rl_present(const void* rgba, int w, int h, int integerScaling, int crt);
void rl_toggle_borderless(void);
void rl_set_window_size(int w, int h);
void rl_set_target_fps(int fps);
void rl_open_url(const char* url);
void rl_message_box(const char* title, const char* text);
const char* rl_app_dir(void);
int rl_monitor_w(void);
const char* rl_get_clipboard(void);
void rl_set_clipboard(const char* s);
int rl_monitor_h(void);
// audio
void rl_audio_init(void);
void rl_audio_close(void);
int rl_sound_load(const char* path);
void rl_sound_play(int id, float vol, float pitch);
void rl_audio_unload_all(void);
int rl_music_play(const char* path, float vol, int loop);
void rl_music_stop(void);
void rl_music_volume(float vol);
int rl_music_playing(void);
void rl_music_update(void);
int rl_ambient_play(const char* path, float vol);   // a second looping stream (ambience)
void rl_ambient_stop(void);
void rl_ambient_volume(float vol);

#ifdef __cplusplus
}
#endif
