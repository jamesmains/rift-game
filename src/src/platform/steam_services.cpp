// Steamworks implementation of PlatformServices. Compiled into the game only with -DTRIFFALL_STEAM
// (CMake option TRIFFALL_STEAM=ON, with STEAM_SDK pointing at the extracted Steamworks SDK).
// Without that define this file only provides the NullServices instance.
#include "../core/platform_services.h"
#include <cstdio>

static PlatformServices gNullServices;
PlatformServices* gPlatform = &gNullServices;

#ifdef TRIFFALL_STEAM
#include "steam/steam_api.h"

struct SteamServices : PlatformServices {
    bool ok = false, overlay = false, quitRequested = false;
    STEAM_CALLBACK(SteamServices, onOverlay, GameOverlayActivated_t);
    STEAM_CALLBACK(SteamServices, onStatsReceived, UserStatsReceived_t);
    STEAM_CALLBACK(SteamServices, onTextDismissed, GamepadTextInputDismissed_t);
    bool textOpen = false, textDone = false, textCancelled = false; std::string textOut;
    bool statsReady = false;
    bool init() override {
        // When launched outside Steam this restarts the game through the client and returns true -> we must exit.
        if (SteamAPI_RestartAppIfNecessary(TRIFFALL_STEAM_APPID)) { quitRequested = true; return false; }
        ok = SteamAPI_Init();
        if (!ok) { fprintf(stderr, "SteamAPI_Init failed (is Steam running? is steam_appid.txt next to the exe for dev builds?)\n"); return true; } // keep running without Steam
        SteamUserStats()->RequestCurrentStats();
        return true;
    }
    void shutdown() override { if (ok) { SteamUserStats()->StoreStats(); SteamAPI_Shutdown(); ok = false; } }
    void update() override { if (ok) SteamAPI_RunCallbacks(); }
    bool available() const override { return ok; }
    std::string platformName() const override { return "steam"; }
    std::string userName() const override { return ok ? SteamFriends()->GetPersonaName() : ""; }
    void unlockAchievement(const std::string& id) override { if (!ok || !statsReady) return; SteamUserStats()->SetAchievement(id.c_str()); SteamUserStats()->StoreStats(); }
    void setStat(const std::string& id, int value) override { if (ok && statsReady) SteamUserStats()->SetStat(id.c_str(), (int32)value); }
    void storeStats() override { if (ok && statsReady) SteamUserStats()->StoreStats(); }
    void setRichPresence(const std::string& key, const std::string& value) override { if (ok) SteamFriends()->SetRichPresence(key.c_str(), value.c_str()); }
    bool overlayActive() const override { return overlay; }
    bool wantsQuit() const override { return quitRequested; }
    void indicateAchievementProgress(const std::string& id, int cur, int max) override { if (ok && statsReady) SteamUserStats()->IndicateAchievementProgress(id.c_str(), (uint32)cur, (uint32)max); }
    // Big Picture / Steam Deck keyboard; outside those Steam returns false and the game uses its own
    bool showTextInput(const std::string& prompt, const std::string& current, int maxLen) override {
        if (!ok || textOpen) return false;
        textOpen = SteamUtils()->ShowGamepadTextInput(k_EGamepadTextInputModeNormal, k_EGamepadTextInputLineModeSingleLine, prompt.c_str(), (uint32)maxLen, current.c_str());
        textDone = false; return textOpen;
    }
    bool pollTextInput(std::string& out, bool& cancelled) override { if (!textDone) return false; textDone = false; out = textOut; cancelled = textCancelled; return true; }
    bool handheld() const override { return ok && SteamUtils()->IsSteamRunningOnSteamDeck(); }
};
void SteamServices::onTextDismissed(GamepadTextInputDismissed_t* p) {
    textOpen = false; textDone = true; textCancelled = !p->m_bSubmitted; textOut.clear();
    if (p->m_bSubmitted) { uint32 n = SteamUtils()->GetEnteredGamepadTextLength(); std::string buf(n + 1, '\0'); if (SteamUtils()->GetEnteredGamepadTextInput(&buf[0], n + 1)) textOut = buf.c_str(); }
}
void SteamServices::onOverlay(GameOverlayActivated_t* p) { overlay = p->m_bActive != 0; }
void SteamServices::onStatsReceived(UserStatsReceived_t* p) { if (p->m_eResult == k_EResultOK) statsReady = true; }

static SteamServices gSteamServices;
struct SteamInstaller { SteamInstaller() { gPlatform = &gSteamServices; } } gSteamInstaller;
#endif
