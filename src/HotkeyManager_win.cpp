#include "HotkeyManager.h"

#ifdef _WIN32

#include <windows.h>
#include <map>
#include <wx/arrstr.h>

static const wchar_t* kHotkeyWndClass = L"STB_HotkeyManager";

// One entry per live HotkeyManager instance, keyed by its hidden HWND.
static std::map<HWND, HotkeyManager*> s_managers;

static LRESULT CALLBACK HotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_HOTKEY) {
        auto it = s_managers.find(hwnd);
        if (it != s_managers.end())
            it->second->TriggerById(static_cast<uint32_t>(wParam));
        return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

HotkeyManager::HotkeyManager(UrlCallback onTrigger)
    : m_onTrigger(std::move(onTrigger)), m_nextId(1), m_hwnd(nullptr)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = HotkeyWndProc;
    wc.hInstance     = ::GetModuleHandle(NULL);
    wc.lpszClassName = kHotkeyWndClass;
    ::RegisterClassW(&wc);  // Ignored if the class is already registered.

    HWND hwnd = ::CreateWindowExW(0, kHotkeyWndClass, L"",
                                   0, 0, 0, 0, 0,
                                   HWND_MESSAGE, NULL, wc.hInstance, NULL);
    if (hwnd) {
        m_hwnd = hwnd;
        s_managers[hwnd] = this;
    }
}

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
    if (m_hwnd) {
        s_managers.erase(static_cast<HWND>(m_hwnd));
        ::DestroyWindow(static_cast<HWND>(m_hwnd));
        m_hwnd = nullptr;
    }
    ::UnregisterClassW(kHotkeyWndClass, ::GetModuleHandle(NULL));
}

bool HotkeyManager::Register(const wxString& keyCombo, const wxString& url) {
    if (!m_hwnd) return false;

    uint32_t keyCode = 0, modifiers = 0;
    if (!ParseKeyCombo(keyCombo, keyCode, modifiers))
        return false;

    uint32_t id = m_nextId++;
    // MOD_NOREPEAT suppresses repeated WM_HOTKEY messages while the key is held.
    BOOL ok = ::RegisterHotKey(static_cast<HWND>(m_hwnd),
                                static_cast<int>(id),
                                modifiers | MOD_NOREPEAT,
                                keyCode);
    if (!ok) return false;

    m_entries.push_back({ id, url, nullptr });
    return true;
}

void HotkeyManager::UnregisterAll() {
    if (!m_hwnd) return;
    for (const auto& entry : m_entries)
        ::UnregisterHotKey(static_cast<HWND>(m_hwnd), static_cast<int>(entry.id));
    m_entries.clear();
}

void HotkeyManager::TriggerById(uint32_t hotkeyId) {
    for (const auto& entry : m_entries) {
        if (entry.id == hotkeyId) {
            m_onTrigger(entry.url);
            return;
        }
    }
}

bool HotkeyManager::ParseKeyCombo(const wxString& combo, uint32_t& keyCode, uint32_t& modifiers) {
    // Windows VK codes for A–Z are the ASCII values of the uppercase letters.
    // Cmd is mapped to Ctrl — the conventional cross-platform equivalent.
    modifiers = 0;
    keyCode   = 0;
    wxString upper = combo.Upper();
    wxArrayString parts = wxSplit(upper, '+');
    wxString keyName;

    for (size_t i = 0; i < parts.GetCount(); ++i) {
        wxString p = parts[i].Trim().Trim(false);
        if      (p == "CMD" || p == "COMMAND" || p == "CTRL" || p == "CONTROL")
            modifiers |= MOD_CONTROL;
        else if (p == "SHIFT")
            modifiers |= MOD_SHIFT;
        else if (p == "ALT" || p == "OPT" || p == "OPTION")
            modifiers |= MOD_ALT;
        else if (p == "WIN" || p == "WINDOWS")
            modifiers |= MOD_WIN;
        else
            keyName = p;
    }

    if (keyName.Length() == 1) {
        wxUniChar ch = keyName[0];
        if (ch >= 'A' && ch <= 'Z') {
            keyCode = static_cast<uint32_t>(ch.GetValue());  // 'A'=0x41 … 'Z'=0x5A
            return modifiers != 0;
        }
    }
    return false;
}

#endif // _WIN32
