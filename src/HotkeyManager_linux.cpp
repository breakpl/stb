#ifdef __WXGTK__

#include "HotkeyManager.h"
#include <wx/log.h>
#include <gdk/gdk.h>

// X11-specific includes — only available when GDK was built with X11 support.
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#endif

#ifdef GDK_WINDOWING_X11

// Modifier masks for "don't-care" combinatorics.
// We grab each hotkey once per combination so it fires regardless of
// NumLock / CapsLock / ScrollLock state.
static const unsigned int kIgnoreMasks[] = {
    0,
    Mod2Mask,                           // NumLock
    LockMask,                           // CapsLock
    Mod5Mask,                           // ScrollLock
    Mod2Mask | LockMask,
    Mod2Mask | Mod5Mask,
    LockMask | Mod5Mask,
    Mod2Mask | LockMask | Mod5Mask
};
static const size_t kNumIgnoreMasks = sizeof(kIgnoreMasks) / sizeof(kIgnoreMasks[0]);

// Silent X error handler — XGrabKey raises BadAccess when another process
// already owns the combo; we treat that as a graceful registration failure.
static int XErrorHandlerSilent(Display*, XErrorEvent*) { return 0; }

static GdkFilterReturn GdkRootFilter(GdkXEvent* xevent, GdkEvent*, gpointer data) {
    XEvent* ev = static_cast<XEvent*>(xevent);
    if (ev->type == KeyPress) {
        auto* mgr = static_cast<HotkeyManager*>(data);
        // Strip NumLock / CapsLock / ScrollLock before matching so the
        // hotkey fires regardless of their current state.
        unsigned int clean = ev->xkey.state & ~(Mod2Mask | LockMask | Mod5Mask);
        mgr->HandleKeyPress(ev->xkey.keycode, clean);
    }
    return GDK_FILTER_CONTINUE;
}

#endif // GDK_WINDOWING_X11

// ─── Constructor ────────────────────────────────────────────────────────────

HotkeyManager::HotkeyManager(UrlCallback onTrigger)
    : m_onTrigger(std::move(onTrigger))
    , m_nextId(1)
    , m_display(nullptr)
    , m_root(0)
{
#ifdef GDK_WINDOWING_X11
    GdkDisplay* gdkDisplay = gdk_display_get_default();
    if (!gdkDisplay || !GDK_IS_X11_DISPLAY(gdkDisplay)) {
        wxLogWarning("HotkeyManager: global hotkeys require X11; "
                     "running on Wayland or without a display — hotkeys disabled.");
        return;
    }
    m_display = gdk_x11_display_get_xdisplay(gdkDisplay);
    m_root    = DefaultRootWindow(static_cast<Display*>(m_display));

    GdkWindow* root = gdk_get_default_root_window();
    if (root)
        gdk_window_add_filter(root, GdkRootFilter, this);
#else
    wxLogWarning("HotkeyManager: compiled without X11 windowing support — hotkeys disabled.");
#endif
}

// ─── Destructor ─────────────────────────────────────────────────────────────

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
#ifdef GDK_WINDOWING_X11
    GdkWindow* root = gdk_get_default_root_window();
    if (root)
        gdk_window_remove_filter(root, GdkRootFilter, this);
#endif
}

// ─── Register ───────────────────────────────────────────────────────────────

bool HotkeyManager::Register(const wxString& keyCombo, const wxString& url) {
#ifndef GDK_WINDOWING_X11
    return false;
#else
    if (!m_display) return false;

    uint32_t keysym = 0, mods = 0;
    if (!ParseKeyCombo(keyCombo, keysym, mods)) return false;

    Display* dpy = static_cast<Display*>(m_display);
    KeyCode code = XKeysymToKeycode(dpy, static_cast<KeySym>(keysym));
    if (code == 0) {
        wxLogWarning("HotkeyManager: no keycode for keysym 0x%x in combo '%s'",
                     keysym, keyCombo);
        return false;
    }

    // Suppress BadAccess errors; XSync before and after to flush the queue.
    XSync(dpy, False);
    auto* prevHandler = XSetErrorHandler(XErrorHandlerSilent);
    for (size_t i = 0; i < kNumIgnoreMasks; ++i)
        XGrabKey(dpy, code, mods | kIgnoreMasks[i],
                 static_cast<Window>(m_root),
                 True, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
    XSetErrorHandler(prevHandler);

    Entry e;
    e.id      = m_nextId++;
    e.url     = url;
    e.ref     = nullptr;
    e.x11code = code;
    e.x11mods = mods;
    m_entries.push_back(e);
    return true;
#endif
}

// ─── UnregisterAll ──────────────────────────────────────────────────────────

void HotkeyManager::UnregisterAll() {
#ifdef GDK_WINDOWING_X11
    if (m_display) {
        Display* dpy = static_cast<Display*>(m_display);
        auto* prev   = XSetErrorHandler(XErrorHandlerSilent);
        for (const auto& e : m_entries) {
            for (size_t i = 0; i < kNumIgnoreMasks; ++i)
                XUngrabKey(dpy, e.x11code, e.x11mods | kIgnoreMasks[i],
                           static_cast<Window>(m_root));
        }
        XSync(dpy, False);
        XSetErrorHandler(prev);
    }
#endif
    m_entries.clear();
}

// ─── TriggerById ────────────────────────────────────────────────────────────

void HotkeyManager::TriggerById(uint32_t id) {
    for (const auto& e : m_entries) {
        if (e.id == id) {
            m_onTrigger(e.url);
            return;
        }
    }
}

// ─── HandleKeyPress ─────────────────────────────────────────────────────────

void HotkeyManager::HandleKeyPress(uint32_t x11keycode, uint32_t state) {
    for (const auto& e : m_entries) {
        if (e.x11code == x11keycode && e.x11mods == state) {
            m_onTrigger(e.url);
            return;
        }
    }
}

// ─── ParseKeyCombo ──────────────────────────────────────────────────────────

bool HotkeyManager::ParseKeyCombo(const wxString& combo,
                                   uint32_t& keyCode, uint32_t& modifiers) {
    keyCode   = 0;
    modifiers = 0;

    wxArrayString parts = wxSplit(combo.Upper(), '+');
    for (size_t i = 0; i < parts.GetCount(); ++i) {
        wxString p = parts[i].Trim().Trim(false);
        if (p == "CMD" || p == "COMMAND" || p == "CTRL" || p == "CONTROL")
            modifiers |= ControlMask;
        else if (p == "SHIFT")
            modifiers |= ShiftMask;
        else if (p == "ALT" || p == "OPT" || p == "OPTION")
            modifiers |= Mod1Mask;
        else if (p.Length() == 1 && p[0] >= 'A' && p[0] <= 'Z')
            // X11 keysyms for letters equal their lowercase ASCII value (XK_a = 0x61).
            keyCode = static_cast<uint32_t>('a' + (p[0] - 'A'));
        else
            return false; // unknown token
    }
    return keyCode != 0 && modifiers != 0;
}

#endif // __WXGTK__
