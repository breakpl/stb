#include "HotkeyManager.h"

#ifdef __WXOSX__

#import <Carbon/Carbon.h>
#include <wx/arrstr.h>

static const OSType kHotkeySignature = 'STHK';

static OSStatus HotkeyEventHandler(EventHandlerCallRef, EventRef event, void* userData) {
    EventHotKeyID hotKeyID;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                      nullptr, sizeof(hotKeyID), nullptr, &hotKeyID);
    static_cast<HotkeyManager*>(userData)->TriggerById(hotKeyID.id);
    return noErr;
}

HotkeyManager::HotkeyManager(UrlCallback onTrigger)
    : m_handlerRef(nullptr), m_onTrigger(std::move(onTrigger)), m_nextId(1)
{
    EventTypeSpec eventType = { kEventClassKeyboard, kEventHotKeyPressed };
    EventHandlerRef handler = nullptr;
    InstallApplicationEventHandler(
        NewEventHandlerUPP(HotkeyEventHandler), 1, &eventType, this, &handler);
    m_handlerRef = handler;
}

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
    if (m_handlerRef) {
        RemoveEventHandler(static_cast<EventHandlerRef>(m_handlerRef));
        m_handlerRef = nullptr;
    }
}

bool HotkeyManager::Register(const wxString& keyCombo, const wxString& url) {
    uint32_t keyCode = 0, modifiers = 0;
    if (!ParseKeyCombo(keyCombo, keyCode, modifiers))
        return false;

    uint32_t id = m_nextId++;
    EventHotKeyID hotKeyID = { kHotkeySignature, id };
    EventHotKeyRef ref = nullptr;
    OSStatus status = RegisterEventHotKey(
        keyCode, modifiers, hotKeyID, GetApplicationEventTarget(), 0, &ref);
    if (status != noErr)
        return false;

    m_entries.push_back({ id, url, ref });
    return true;
}

void HotkeyManager::UnregisterAll() {
    for (auto& entry : m_entries) {
        if (entry.ref) {
            UnregisterEventHotKey(static_cast<EventHotKeyRef>(entry.ref));
            entry.ref = nullptr;
        }
    }
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

bool HotkeyManager::ParseKeyCombo(const wxString& combo, uint32_t& keyCode, uint32_t& modifiers) { // static
    // Virtual key codes from <HIToolbox/Events.h> — layout-independent physical positions.
    static const struct { const char* name; uint32_t code; } kKeyMap[] = {
        {"A", kVK_ANSI_A}, {"B", kVK_ANSI_B}, {"C", kVK_ANSI_C}, {"D", kVK_ANSI_D},
        {"E", kVK_ANSI_E}, {"F", kVK_ANSI_F}, {"G", kVK_ANSI_G}, {"H", kVK_ANSI_H},
        {"I", kVK_ANSI_I}, {"J", kVK_ANSI_J}, {"K", kVK_ANSI_K}, {"L", kVK_ANSI_L},
        {"M", kVK_ANSI_M}, {"N", kVK_ANSI_N}, {"O", kVK_ANSI_O}, {"P", kVK_ANSI_P},
        {"Q", kVK_ANSI_Q}, {"R", kVK_ANSI_R}, {"S", kVK_ANSI_S}, {"T", kVK_ANSI_T},
        {"U", kVK_ANSI_U}, {"V", kVK_ANSI_V}, {"W", kVK_ANSI_W}, {"X", kVK_ANSI_X},
        {"Y", kVK_ANSI_Y}, {"Z", kVK_ANSI_Z},
    };

    modifiers = 0;
    keyCode   = 0;
    wxString upper = combo.Upper();
    wxArrayString parts = wxSplit(upper, '+');
    wxString keyName;

    for (size_t i = 0; i < parts.GetCount(); ++i) {
        wxString p = parts[i].Trim().Trim(false);
        if      (p == "CMD" || p == "COMMAND")          modifiers |= cmdKey;
        else if (p == "SHIFT")                           modifiers |= shiftKey;
        else if (p == "CTRL" || p == "CONTROL")          modifiers |= controlKey;
        else if (p == "ALT" || p == "OPT" || p == "OPTION") modifiers |= optionKey;
        else                                             keyName = p;
    }

    for (const auto& entry : kKeyMap) {
        if (keyName == entry.name) {
            keyCode = entry.code;
            return modifiers != 0;
        }
    }
    return false;
}

#endif // __WXOSX__
