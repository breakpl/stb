#pragma once
#if defined(__WXOSX__) || defined(_WIN32) || defined(__WXGTK__)

#include <cstdint>
#include <functional>
#include <vector>
#include <wx/string.h>

// Registers global hotkeys that fire regardless of which app is in focus.
// macOS:   Carbon RegisterEventHotKey  — no Accessibility permission required.
// Windows: Win32  RegisterHotKey       — no special permission required.
// Linux:   X11    XGrabKey via GDK     — X11 only; logs a warning on Wayland.
// Combo format: "Cmd+Shift+S" (Cmd/Shift/Ctrl/Alt + A–Z).
// On Windows and Linux, Cmd maps to Ctrl (the conventional cross-platform equivalent).
class HotkeyManager {
public:
    using UrlCallback = std::function<void(const wxString&)>;

    explicit HotkeyManager(UrlCallback onTrigger);
    ~HotkeyManager();

    // Returns false when the combo is invalid or registration fails
    // (e.g. another app already owns that combo).
    bool Register(const wxString& keyCombo, const wxString& url);

    void UnregisterAll();

    // Called by the platform event handler — not for external use.
    void TriggerById(uint32_t hotkeyId);

    // Parses a combo string into a platform key code and modifier flags.
    // Public and static so it can be unit-tested without a running event loop.
    // Returns false when the combo is malformed (unknown key or no modifiers).
    static bool ParseKeyCombo(const wxString& combo, uint32_t& keyCode, uint32_t& modifiers);

#ifdef __WXGTK__
    // Called by the GDK root-window event filter.
    void HandleKeyPress(uint32_t x11keycode, uint32_t state);
#endif

private:
    struct Entry {
        uint32_t id;
        wxString url;
        void*    ref;       // EventHotKeyRef (macOS) — unused on other platforms
#ifdef __WXGTK__
        uint32_t x11code;   // resolved X11 keycode
        uint32_t x11mods;   // base modifier mask (without NumLock/CapsLock variants)
#endif
    };

    std::vector<Entry> m_entries;
    UrlCallback m_onTrigger;
    uint32_t    m_nextId;

#ifdef __WXOSX__
    void* m_handlerRef;     // EventHandlerRef
#endif
#ifdef _WIN32
    void* m_hwnd;           // HWND of the hidden message-only window
#endif
#ifdef __WXGTK__
    void* m_display;        // Display* — null when X11 is unavailable
    unsigned long m_root;   // root Window id
#endif
};

#endif // __WXOSX__ || _WIN32 || __WXGTK__
