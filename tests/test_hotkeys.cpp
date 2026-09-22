// Tests for HotkeyManager::ParseKeyCombo.
// Carbon is pure-C so it compiles in a plain .cpp file.
// The test target is Apple-only (see CMakeLists.txt).

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <wx/init.h>
#include <wx/log.h>
#include <Carbon/Carbon.h>

#include "HotkeyManager.h"

// ── valid combos ──────────────────────────────────────────────────────────────

TEST_CASE("ParseKeyCombo: Cmd+Shift+S", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Cmd+Shift+S", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_S);
    REQUIRE(mod == (uint32_t)(cmdKey | shiftKey));
}

TEST_CASE("ParseKeyCombo: Cmd+Shift+J", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Cmd+Shift+J", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_J);
    REQUIRE(mod == (uint32_t)(cmdKey | shiftKey));
}

TEST_CASE("ParseKeyCombo: Cmd+Shift+A", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Cmd+Shift+A", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_A);
    REQUIRE(mod == (uint32_t)(cmdKey | shiftKey));
}

TEST_CASE("ParseKeyCombo: Cmd+Shift+Z", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Cmd+Shift+Z", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_Z);
    REQUIRE(mod == (uint32_t)(cmdKey | shiftKey));
}

TEST_CASE("ParseKeyCombo: Cmd+Alt+T", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Cmd+Alt+T", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_T);
    REQUIRE(mod == (uint32_t)(cmdKey | optionKey));
}

TEST_CASE("ParseKeyCombo: Ctrl+Shift+P", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE(HotkeyManager::ParseKeyCombo("Ctrl+Shift+P", key, mod));
    REQUIRE(key == (uint32_t)kVK_ANSI_P);
    REQUIRE(mod == (uint32_t)(controlKey | shiftKey));
}

// ── case-insensitivity ────────────────────────────────────────────────────────

TEST_CASE("ParseKeyCombo: lowercase tokens parse identically", "[hotkey][parse]") {
    uint32_t key1 = 0, mod1 = 0;
    uint32_t key2 = 0, mod2 = 0;
    HotkeyManager::ParseKeyCombo("Cmd+Shift+S", key1, mod1);
    HotkeyManager::ParseKeyCombo("cmd+shift+s", key2, mod2);
    REQUIRE(key1 == key2);
    REQUIRE(mod1 == mod2);
}

TEST_CASE("ParseKeyCombo: mixed case tokens parse identically", "[hotkey][parse]") {
    uint32_t key1 = 0, mod1 = 0;
    uint32_t key2 = 0, mod2 = 0;
    HotkeyManager::ParseKeyCombo("CMD+SHIFT+J", key1, mod1);
    HotkeyManager::ParseKeyCombo("Cmd+Shift+J", key2, mod2);
    REQUIRE(key1 == key2);
    REQUIRE(mod1 == mod2);
}

TEST_CASE("ParseKeyCombo: COMMAND alias parses same as CMD", "[hotkey][parse]") {
    uint32_t key1 = 0, mod1 = 0;
    uint32_t key2 = 0, mod2 = 0;
    HotkeyManager::ParseKeyCombo("Command+Shift+A", key1, mod1);
    HotkeyManager::ParseKeyCombo("Cmd+Shift+A",     key2, mod2);
    REQUIRE(mod1 == mod2);
    REQUIRE(key1 == key2);
}

TEST_CASE("ParseKeyCombo: OPTION alias parses same as ALT", "[hotkey][parse]") {
    uint32_t key1 = 0, mod1 = 0;
    uint32_t key2 = 0, mod2 = 0;
    HotkeyManager::ParseKeyCombo("Cmd+Option+K", key1, mod1);
    HotkeyManager::ParseKeyCombo("Cmd+Alt+K",    key2, mod2);
    REQUIRE(mod1 == mod2);
    REQUIRE(key1 == key2);
}

TEST_CASE("ParseKeyCombo: OPT alias parses same as ALT", "[hotkey][parse]") {
    uint32_t key1 = 0, mod1 = 0;
    uint32_t key2 = 0, mod2 = 0;
    HotkeyManager::ParseKeyCombo("Cmd+Opt+K", key1, mod1);
    HotkeyManager::ParseKeyCombo("Cmd+Alt+K", key2, mod2);
    REQUIRE(mod1 == mod2);
    REQUIRE(key1 == key2);
}

// ── invalid / malformed combos ────────────────────────────────────────────────

TEST_CASE("ParseKeyCombo: returns false with no modifier", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE_FALSE(HotkeyManager::ParseKeyCombo("S", key, mod));
}

TEST_CASE("ParseKeyCombo: returns false with no key letter", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE_FALSE(HotkeyManager::ParseKeyCombo("Cmd+Shift", key, mod));
}

TEST_CASE("ParseKeyCombo: returns false for unknown key name", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE_FALSE(HotkeyManager::ParseKeyCombo("Cmd+Shift+F12", key, mod));
}

TEST_CASE("ParseKeyCombo: returns false for empty string", "[hotkey][parse]") {
    uint32_t key = 0, mod = 0;
    REQUIRE_FALSE(HotkeyManager::ParseKeyCombo("", key, mod));
}

// ── entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    wxLog::EnableLogging(false);
    wxInitialize();
    int result = Catch::Session().run(argc, argv);
    wxUninitialize();
    return result;
}
