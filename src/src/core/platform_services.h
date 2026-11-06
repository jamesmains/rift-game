// Platform services (achievements, stats, rich presence, overlay) behind one interface.
// The default implementation does nothing; src/platform/steam_services.cpp provides the Steamworks one
// when the game is built with -DTRIFFALL_STEAM (see STEAM.md). Game code never includes steam_api.h.
#pragma once
#include <string>

struct PlatformServices {
    virtual ~PlatformServices() {}
    virtual bool init() { return true; }                 // false = platform refused (e.g. Steam asked us to relaunch through the client)
    virtual void shutdown() {}
    virtual void update() {}                              // run callbacks, once per frame
    virtual bool available() const { return false; }      // true when a real platform is connected
    virtual std::string platformName() const { return "none"; }
    virtual std::string userName() const { return ""; }   // "" = unknown, the game falls back to its own name prompt
    virtual void unlockAchievement(const std::string& id) { (void)id; }
    virtual void setStat(const std::string& id, int value) { (void)id; (void)value; }
    virtual void storeStats() {}
    virtual void setRichPresence(const std::string& key, const std::string& value) { (void)key; (void)value; }
    virtual bool overlayActive() const { return false; }  // the game pauses while the overlay is open
    virtual bool wantsQuit() const { return false; }      // platform asked the game to close
    virtual void indicateAchievementProgress(const std::string& id, int cur, int max) { (void)id; (void)cur; (void)max; }
    // the device's own text keyboard (Steam Deck / Big Picture). showTextInput returns false when there is none - the
    // game then opens its own on-screen keyboard. pollTextInput returns true once, when the player closed it (out = text).
    virtual bool showTextInput(const std::string& prompt, const std::string& current, int maxLen) { (void)prompt; (void)current; (void)maxLen; return false; }
    virtual bool pollTextInput(std::string& out, bool& cancelled) { (void)out; (void)cancelled; return false; }
    virtual bool handheld() const { return false; }       // running on a handheld (Steam Deck): bigger text could follow this
};
extern PlatformServices* gPlatform;   // never null (points at a NullServices when nothing is connected)
