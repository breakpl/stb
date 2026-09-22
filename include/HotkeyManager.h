#pragma once
#ifdef __WXOSX__

#include <cstdint>
#include <functional>
#include <vector>
#include <wx/string.h>

// Registers global Carbon hotkeys (no Accessibility permission required).
// Each hotkey maps a key combo string like "Cmd+Shift+S" to a URL that is
// passed to the callback when the user presses the combo.
class HotkeyManager {
public:
    using UrlCallback = std::function<void(const wxString&)>;

    explicit HotkeyManager(UrlCallback onTrigger);
    ~HotkeyManager();

    // keyCombo: "Cmd+Shift+S" (supports Cmd/Shift/Ctrl/Alt + A–Z).
    // Returns false when the combo cannot be parsed or registration fails
    // (e.g. another app already owns that combo).
    bool Register(const wxString& keyCombo, const wxString& url);

    void UnregisterAll();

    // Called by the Carbon event handler — not for external use.
    void TriggerById(uint32_t hotkeyId);

    // Parses "Cmd+Shift+S" into a Carbon virtual keyCode and modifier flags.
    // Public and static so it can be unit-tested without a running event loop.
    // Returns false when the combo is malformed (unknown key or no modifiers).
    static bool ParseKeyCombo(const wxString& combo, uint32_t& keyCode, uint32_t& modifiers);

private:

    struct Entry {
        uint32_t id;
        wxString url;
        void*    ref;   // EventHotKeyRef stored as void* to keep Carbon out of the header
    };

    std::vector<Entry> m_entries;
    void*       m_handlerRef;   // EventHandlerRef
    UrlCallback m_onTrigger;
    uint32_t    m_nextId;
};

#endif // __WXOSX__
