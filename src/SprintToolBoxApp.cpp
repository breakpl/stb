#include "SprintToolBoxApp.h"
#include "ConverterDialog.h"
#include "TimeConverterDialog.h"
#include "Base64Dialog.h"
#include "UrlEncoderDialog.h"
#include "JsonFormatterDialog.h"
#include "CustomizeMenuDialog.h"
#include "JiraService.h"
#include "UpdateService.h"
#include "Config.h"
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/icon.h>
#include <wx/bitmap.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/utils.h>
#include <curl/curl.h>
#include <string>
#include <thread>

#if defined(__linux__)
#include <sys/stat.h>
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif
#ifndef UPDATE_REPO_OWNER
#define UPDATE_REPO_OWNER "breakpl"
#endif
#ifndef UPDATE_REPO_NAME
#define UPDATE_REPO_NAME "stb"
#endif

#ifdef _WIN32
#include <windows.h>
#include <string>
#endif

#ifdef __WXOSX__
#include <wx/osx/private.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <ServiceManagement/ServiceManagement.h>

// Observes system theme changes and forwards them to the C++ owner.
@interface ThemeChangeObserver : NSObject
@property (assign) SprintToolBoxApp* owner;
@end

@implementation ThemeChangeObserver
- (void)themeChanged:(NSNotification*)notification {
    if (self.owner)
        self.owner->UpdateTrayIcon(self.owner->m_currentIconText, self.owner->m_currentDaysPassed);
}
@end

// Receives NSStatusItem button clicks and forwards them to ShowContextMenu().
@interface StatusItemClickHandler : NSObject
@property (assign) SprintToolBoxApp* owner;
@end

@implementation StatusItemClickHandler
- (void)clicked:(id)sender {
    if (self.owner) self.owner->ShowContextMenu();
}
@end

#endif

// Constants for menu bar icon
static const int ICON_FONT_SIZE_MAC = 14;  // SF Mono Regular – sized for attributedTitle
static const int SPRINT_UPDATE_INTERVAL_MS  = 300000; // 5 minutes
static const int NETWORK_RETRY_INTERVAL_MS  = 30000;  // 30 s after network/parse failure
static const int UPDATE_CHECK_INITIAL_MS    = 30000;  // 30 s after launch
static const int UPDATE_CHECK_INTERVAL_MS   = 6 * 60 * 60 * 1000; // 6 hours

#ifdef _WIN32
// ── Windows theme-change observer ────────────────────────────────────────────
// A tiny invisible window whose sole job is to receive WM_SETTINGCHANGE with
// "ImmersiveColorSet" (sent when the user switches light/dark/accent themes)
// and forward it to our SprintToolBoxApp so the tray icon repaints.
static const wchar_t* THEME_WND_CLASS = L"STB_ThemeWatcher";

static LRESULT CALLBACK ThemeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SETTINGCHANGE && lParam) {
        // lParam is a pointer to the string identifying what changed
        if (wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
            auto* app = reinterpret_cast<SprintToolBoxApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (app) {
                app->UpdateTrayIcon(app->m_currentIconText, app->m_currentDaysPassed);
            }
        }
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

wxBEGIN_EVENT_TABLE(SprintToolBoxApp, wxTaskBarIcon)
    EVT_MENU(ID_COPY_UNIX, SprintToolBoxApp::OnCopyUnixTimestamp)
    EVT_MENU(ID_COPY_ZULU, SprintToolBoxApp::OnCopyZuluTimestamp)
    EVT_MENU(ID_OPEN_CONVERTER, SprintToolBoxApp::OnOpenConverter)
    EVT_MENU(ID_OPEN_TIME_CONVERTER, SprintToolBoxApp::OnOpenTimeConverter)
    EVT_MENU(ID_OPEN_BASE64_ENCODER, SprintToolBoxApp::OnOpenBase64Encoder)
    EVT_MENU(ID_OPEN_URL_ENCODER, SprintToolBoxApp::OnOpenUrlEncoder)
    EVT_MENU(ID_OPEN_JSON_FORMATTER, SprintToolBoxApp::OnOpenJsonFormatter)
    EVT_MENU(ID_QUIT, SprintToolBoxApp::OnQuit)
    EVT_MENU(ID_CHECK_UPDATES, SprintToolBoxApp::OnCheckUpdatesMenu)
    EVT_MENU(ID_TOGGLE_AUTOSTART, SprintToolBoxApp::OnToggleAutostart)
    EVT_MENU(ID_CUSTOMIZE_MENU,  SprintToolBoxApp::OnCustomizeMenu)
    EVT_TIMER(ID_SPRINT_TIMER, SprintToolBoxApp::OnSprintUpdateTimer)
    EVT_TIMER(ID_CONFIG_WATCH_TIMER, SprintToolBoxApp::OnConfigWatchTimer)
    EVT_TIMER(ID_UPDATE_CHECK_TIMER, SprintToolBoxApp::OnUpdateCheckTimer)
    EVT_TIMER(ID_RETRY_TIMER, SprintToolBoxApp::OnRetryTimer)

    EVT_MENU_RANGE(ID_DYNAMIC_MENU_START, ID_DYNAMIC_MENU_START + 999, SprintToolBoxApp::OnDynamicMenuClick)
    // macOS clicks are routed through StatusItemClickHandler; only Linux/Windows need this.
#ifndef __WXOSX__
    EVT_TASKBAR_LEFT_UP(SprintToolBoxApp::OnTaskBarClick)
#endif
#ifdef _WIN32
    EVT_TASKBAR_RIGHT_UP(SprintToolBoxApp::OnTaskBarClick)
#endif
wxEND_EVENT_TABLE()

SprintToolBoxApp::SprintToolBoxApp() 
    : wxTaskBarIcon()
    , m_converterDialog(nullptr)
    , m_timeConverterDialog(nullptr)
    , m_base64Dialog(nullptr)
    , m_urlEncoderDialog(nullptr)
    , m_jsonFormatterDialog(nullptr)
    , m_customizeMenuDialog(nullptr)
    , m_updateService(nullptr)
    , m_config(nullptr)
    , m_sprintUpdateTimer(nullptr)
    , m_configWatchTimer(nullptr)
    , m_updateCheckTimer(nullptr)
    , m_retryTimer(nullptr)
    , m_currentIconText("...")
    , m_currentDaysPassed(-1)
    , m_menuShowing(false)
    , m_pendingUpdateVersion()
#ifdef __WXOSX__
    , m_themeObserver(nullptr)
    , m_statusItem(nullptr)
    , m_statusItemHandler(nullptr)
#endif
#ifdef _WIN32
    , m_themeHwnd(nullptr)
#endif
{
    // Initialize configuration
    m_config = & Config::GetInstance();

    // Setup timer for periodic sprint updates
    m_sprintUpdateTimer = new wxTimer(this, ID_SPRINT_TIMER);
    m_sprintUpdateTimer->Start(SPRINT_UPDATE_INTERVAL_MS);

    m_retryTimer = new wxTimer(this, ID_RETRY_TIMER);

    // Updater
    m_updateService = new UpdateService(UPDATE_REPO_OWNER, UPDATE_REPO_NAME);
    m_updateCheckTimer = new wxTimer(this, ID_UPDATE_CHECK_TIMER);
    m_updateCheckTimer->Start(UPDATE_CHECK_INITIAL_MS, wxTIMER_ONE_SHOT);

    // Config-file watcher: poll the INI modification time every 10 s so
    // that credential / setting changes are picked up promptly.
    m_configWatchTimer = new wxTimer(this, ID_CONFIG_WATCH_TIMER);
    m_configWatchTimer->Start(10000);
    
#ifdef __WXOSX__
    // Setup theme change observer
    if (@available(macOS 10.14, *)) {
        ThemeChangeObserver* observer = [[ThemeChangeObserver alloc] init];
        observer.owner = this;
        m_themeObserver = (void*)CFBridgingRetain(observer);
        
        [[NSDistributedNotificationCenter defaultCenter] addObserver:observer
            selector:@selector(themeChanged:)
            name:@"AppleInterfaceThemeChangedNotification"
            object:nil];
    }
#endif

#ifdef _WIN32
    // Create a hidden message-only window to receive WM_SETTINGCHANGE
    // when the user switches Windows light/dark/accent theme.
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = ThemeWndProc;
        wc.hInstance     = ::GetModuleHandle(NULL);
        wc.lpszClassName = THEME_WND_CLASS;
        ::RegisterClassW(&wc);
        m_themeHwnd = ::CreateWindowExW(0, THEME_WND_CLASS, L"",
                                        0, 0, 0, 0, 0,
                                        HWND_MESSAGE, NULL, wc.hInstance, NULL);
        if (m_themeHwnd) {
            ::SetWindowLongPtr(m_themeHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }
#endif

    // Defer both the initial icon paint and the first sprint fetch until
    // after the wx event loop has started.  Shell_NotifyIcon (Windows) and
    // NSStatusBar (macOS) can silently fail when called this early.
    UpdateTimestamps();
    CallAfter([this]() {
        UpdateTrayIcon("...");
        UpdateSprint();
    });
}

SprintToolBoxApp::~SprintToolBoxApp() {
#ifdef __WXOSX__
    if (m_statusItem) {
        NSStatusItem* item = (__bridge NSStatusItem*)m_statusItem;
        [[NSStatusBar systemStatusBar] removeStatusItem:item];
        (void)CFBridgingRelease(m_statusItem);
        m_statusItem = nullptr;
    }

    if (m_statusItemHandler) {
        (void)CFBridgingRelease(m_statusItemHandler);
        m_statusItemHandler = nullptr;
    }

    if (m_themeObserver) {
        ThemeChangeObserver* observer = (__bridge ThemeChangeObserver*)m_themeObserver;
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:observer];
        (void)CFBridgingRelease(m_themeObserver);
        m_themeObserver = nullptr;
    }
#endif

#ifdef _WIN32
    if (m_themeHwnd) {
        ::DestroyWindow(m_themeHwnd);
        ::UnregisterClassW(THEME_WND_CLASS, ::GetModuleHandle(NULL));
        m_themeHwnd = nullptr;
    }
#endif

    if (m_sprintUpdateTimer) {
        m_sprintUpdateTimer->Stop();
        delete m_sprintUpdateTimer;
        m_sprintUpdateTimer = nullptr;
    }

    if (m_configWatchTimer) {
        m_configWatchTimer->Stop();
        delete m_configWatchTimer;
        m_configWatchTimer = nullptr;
    }

    if (m_updateCheckTimer) {
        m_updateCheckTimer->Stop();
        delete m_updateCheckTimer;
        m_updateCheckTimer = nullptr;
    }

    if (m_updateService) {
        delete m_updateService;
        m_updateService = nullptr;
    }
    
    if (m_converterDialog) {
        m_converterDialog->Destroy();
        m_converterDialog = nullptr;
    }

    if (m_timeConverterDialog) {
        m_timeConverterDialog->Destroy();
        m_timeConverterDialog = nullptr;
    }

    if (m_base64Dialog) {
        m_base64Dialog->Destroy();
        m_base64Dialog = nullptr;
    }

    if (m_urlEncoderDialog) {
        m_urlEncoderDialog->Destroy();
        m_urlEncoderDialog = nullptr;
    }

    if (m_jsonFormatterDialog) {
        m_jsonFormatterDialog->Destroy();
        m_jsonFormatterDialog = nullptr;
    }

    if (m_customizeMenuDialog) {
        m_customizeMenuDialog->Destroy();
        m_customizeMenuDialog = nullptr;
    }
}

void SprintToolBoxApp::UpdateTrayIcon(const wxString& text, int daysPassed) {
    // Store current values for theme change updates
    m_currentIconText = text;
    m_currentDaysPassed = daysPassed;
    
#ifdef __WXOSX__
    // On macOS, use NSStatusItem's attributedTitle for native text rendering.
    // This gives automatic dark/light mode adaptation and proper font rendering.
    @autoreleasepool {
        // Create tooltip with sprint info
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) {
            tooltip += wxString::Format(" (Day %d)", daysPassed);
        }
        if (!m_pendingUpdateVersion.IsEmpty()) tooltip += " | v" + m_pendingUpdateVersion + " available";
        
        // First call: create our own NSStatusItem directly.
        // The old approach used SetIcon() + a private KVC valueForKey:@"statusItems"
        // lookup which stopped working on macOS 15 (Sequoia).
        if (!m_statusItem) {
            StatusItemClickHandler* handler = [[StatusItemClickHandler alloc] init];
            handler.owner = this;
            m_statusItemHandler = (void*)CFBridgingRetain(handler);

            NSStatusItem* item = [[NSStatusBar systemStatusBar]
                                  statusItemWithLength:NSVariableStatusItemLength];
            item.button.target  = handler;
            item.button.action  = @selector(clicked:);
            // Trigger on both mouse buttons so right-click also shows the menu.
            [item.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];

            m_statusItem = (void*)CFBridgingRetain(item);
        }
        
        NSStatusItem* targetItem = m_statusItem ? (__bridge NSStatusItem*)m_statusItem : nil;
        if (targetItem) {
            // Get SF Mono font (Apple's monospace font)
            NSFont* font = [NSFont fontWithName:@"SFMono-Regular" size:ICON_FONT_SIZE_MAC];
            if (!font) {
                font = [NSFont monospacedSystemFontOfSize:ICON_FONT_SIZE_MAC weight:NSFontWeightRegular];
            }
            
            NSString* nsText = [NSString stringWithUTF8String:text.utf8_str()];
            
            // Set attributed title - no foreground color so macOS uses the
            // appropriate menu bar text color for light/dark mode automatically
            NSDictionary* attributes = @{
                NSFontAttributeName: font,
            };
            NSAttributedString* attrStr = [[NSAttributedString alloc] initWithString:nsText
                                                                          attributes:attributes];
            
            // Clear any bitmap image, use native text title instead
            targetItem.button.image = nil;
            targetItem.button.attributedTitle = attrStr;
            
            // Give the status item enough width for the text
            targetItem.length = NSVariableStatusItemLength;
            
            // Update tooltip
            NSString* nsTooltip = [NSString stringWithUTF8String:tooltip.utf8_str()];
            targetItem.button.toolTip = nsTooltip;
        }
    }
#elif defined(_WIN32)
    // Windows: render text into a small system-tray icon.
    // Technique: draw WHITE text on a BLACK 32-bit DIB so that each pixel's
    // luminance becomes the alpha coverage.  Then build a wxImage with the
    // desired foreground colour + that alpha and convert via wxIcon.
    {
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) tooltip += wxString::Format(" (Day %d)", daysPassed);
        if (!m_pendingUpdateVersion.IsEmpty()) tooltip += " | v" + m_pendingUpdateVersion + " available";

        // System small-icon metric (respects DPI on per-monitor builds).
        int iconSize = ::GetSystemMetrics(SM_CXSMICON);
        if (iconSize < 16) iconSize = 16;

        // --- Detect dark / light taskbar via registry ---
        bool darkTaskbar = true;
        {
            HKEY hKey = nullptr;
            if (::RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD value = 0, size = sizeof(value);
                if (::RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr,
                        nullptr, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
                    darkTaskbar = (value == 0);
                }
                ::RegCloseKey(hKey);
            }
        }
        BYTE fgR = darkTaskbar ? 255 : 0;
        BYTE fgG = darkTaskbar ? 255 : 0;
        BYTE fgB = darkTaskbar ? 255 : 0;

        // --- Step 1: Create a 32-bit top-down DIB section ---
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = iconSize;
        bmi.bmiHeader.biHeight      = -iconSize;  // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC screenDC = ::GetDC(nullptr);
        HBITMAP hBmp = ::CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS,
                                          &bits, nullptr, 0);
        if (!hBmp || !bits) {
            ::ReleaseDC(nullptr, screenDC);
            return;
        }
        HDC memDC = ::CreateCompatibleDC(screenDC);
        if (!memDC) {
            ::DeleteObject(hBmp);
            ::ReleaseDC(nullptr, screenDC);
            return;
        }
        HBITMAP oldBmp = (HBITMAP)::SelectObject(memDC, hBmp);

        // Clear to black (all channels zero → fully transparent)
        memset(bits, 0, iconSize * iconSize * 4);

        // --- Step 2: Auto-size font so the text fits the icon ---
        // Start large and shrink until the measured text extent fits inside
        // the icon with 1 px padding on each side.
        int textLen = (int)text.length();
        int fontSize;
        if (textLen <= 2)      fontSize = iconSize - 3;
        else if (textLen <= 3) fontSize = iconSize - 5;
        else                   fontSize = iconSize - 7;
        if (fontSize < 6) fontSize = 6;

        HFONT hFont = ::CreateFontW(
            -fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial Narrow");
        HFONT oldFont = (HFONT)::SelectObject(memDC, hFont);

        // Draw WHITE text so any pixel brightness = coverage
        ::SetBkMode(memDC, TRANSPARENT);
        ::SetTextColor(memDC, RGB(255, 255, 255));

        RECT rc = { 0, 0, iconSize, iconSize };
        ::DrawTextW(memDC, text.wc_str(), -1, &rc,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);

        ::GdiFlush();                          // commit pixel data before reading

        // Clean up GDI objects (keep hBmp alive – bits pointer still valid)
        ::SelectObject(memDC, oldBmp);
        ::DeleteDC(memDC);
        if (hFont) ::DeleteObject(hFont);
        ::ReleaseDC(nullptr, screenDC);

        // --- Step 4: Build wxImage from DIB luminance → alpha ---
        wxImage img(iconSize, iconSize);
        img.InitAlpha();

        BYTE* px = (BYTE*)bits;
        for (int y = 0; y < iconSize; y++) {
            for (int x = 0; x < iconSize; x++, px += 4) {
                // max(R,G,B) gives the true coverage even under ClearType
                BYTE alpha = px[2];                    // R
                if (px[1] > alpha) alpha = px[1];      // G
                if (px[0] > alpha) alpha = px[0];      // B

                img.SetRGB(x, y, fgR, fgG, fgB);
                img.SetAlpha(x, y, alpha);
            }
        }

        ::DeleteObject(hBmp);                  // done reading bits

        // --- Step 5: wxImage → wxBitmap → wxIcon → tray ---
        wxBitmap bmp(img, 32);
        wxIcon icon;
        icon.CopyFromBitmap(bmp);
        m_trayIconCurrent = icon;
        SetIcon(m_trayIconCurrent, tooltip);
    }
#else
    // Linux/generic fallback: simple wxWidgets bitmap icon
    {
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) tooltip += wxString::Format(" (Day %d)", daysPassed);
        if (!m_pendingUpdateVersion.IsEmpty()) tooltip += " | v" + m_pendingUpdateVersion + " available";

        const int iconSize = 22;
        wxBitmap bmp(iconSize, iconSize, 32);
        wxMemoryDC dc;
        dc.SelectObject(bmp);
        dc.SetBackground(*wxTRANSPARENT_BRUSH);
        dc.Clear();

        dc.SetFont(wxFont(wxFontInfo(8).Bold().FaceName("Monospace")));
        dc.SetTextForeground(*wxWHITE);
        wxSize textSize = dc.GetTextExtent(text);
        int x = (iconSize - textSize.GetWidth()) / 2;
        int y = (iconSize - textSize.GetHeight()) / 2;
        dc.DrawText(text, x, y);

        dc.SelectObject(wxNullBitmap);
        wxIcon icon;
        icon.CopyFromBitmap(bmp);
        m_trayIconCurrent = icon;
        SetIcon(m_trayIconCurrent, tooltip);
    }
#endif
}

void SprintToolBoxApp::UpdateTimestamps() {
    wxDateTime now = wxDateTime::Now();
    
    // Unix timestamp
    m_unixTimestamp = wxString::Format("%lld", now.GetValue().GetValue() / 1000);
    
    // Zulu (ISO 8601) timestamp
    wxDateTime utcNow = now.ToUTC();
    m_zuluTimestamp = utcNow.FormatISOCombined('T') + "Z";
}

wxMenu* SprintToolBoxApp::CreatePopupMenu() {
#ifdef __WXOSX__
    // On macOS we own the NSStatusItem directly and show the menu from
    // ShowContextMenu(), so wxWidgets never needs to call this.
    return nullptr;
#elif defined(__WXGTK__)
    // On Linux/GTK, CreatePopupMenu() is called for right-click by default
    return BuildPopupMenu();
#else
    // On Windows we handle the popup manually via OnTaskBarClick()
    // so that we can apply the SetForegroundWindow fix (KB Q135788).
    return nullptr;
#endif
}

void SprintToolBoxApp::OnTaskBarClick(wxTaskBarIconEvent& event) {
#ifdef _WIN32
    ShowContextMenu();
#elif defined(__WXGTK__)
    ShowContextMenu();
#endif
    // On macOS, CreatePopupMenu() handles everything — this handler is a no-op.
}

void SprintToolBoxApp::ShowContextMenu() {
    // Guard against re-entrant calls (DOWN+UP, rapid double-click, etc.)
    if (m_menuShowing) return;
    m_menuShowing = true;

    wxMenu* menu = BuildPopupMenu();
    if (!menu) { m_menuShowing = false; return; }

    // Pause timers that trigger synchronous HTTP so they can't block
    // the main thread while the popup is visible.
    bool sprintWasRunning = m_sprintUpdateTimer && m_sprintUpdateTimer->IsRunning();
    bool configWasRunning = m_configWatchTimer  && m_configWatchTimer->IsRunning();

    if (sprintWasRunning)   m_sprintUpdateTimer->Stop();
    if (configWasRunning)   m_configWatchTimer->Stop();


#ifdef _WIN32
    // KB Q135788 – TrackPopupMenu requires the owner window to be the
    // foreground window.  Temporarily attach our thread input to the
    // current foreground thread so SetForegroundWindow succeeds.
    HWND fgWnd = ::GetForegroundWindow();
    DWORD fgThread = fgWnd ? ::GetWindowThreadProcessId(fgWnd, NULL) : 0;
    DWORD myThread = ::GetCurrentThreadId();
    bool attached = false;
    if (fgThread && fgThread != myThread) {
        attached = (::AttachThreadInput(fgThread, myThread, TRUE) != 0);
    }
#endif

#ifdef __WXOSX__
    // Use native Cocoa popup anchored below our NSStatusItem button.
    // popUpMenuPositioningItem: is synchronous – it blocks until dismissed.
    if (m_statusItem) {
        NSStatusItem* nsItem = (__bridge NSStatusItem*)m_statusItem;
        // GetHMenu() returns the underlying NSMenu* that wxWidgets built.
        NSMenu* nsMenu = (__bridge NSMenu*)(void*)menu->GetHMenu();
        if (nsMenu) {
            // Without this, clickedAction: dispatches wxCommandEvent to the
            // wxMenu itself, which has no handler table.  Setting us as the
            // next handler in the chain lets the event reach our event table.
            menu->SetNextHandler(this);
            [nsMenu popUpMenuPositioningItem:nil
                                  atLocation:NSMakePoint(0, nsItem.button.bounds.size.height)
                                      inView:nsItem.button];
            menu->SetNextHandler(nullptr);
        }
    }
#else
    PopupMenu(menu);
#endif

#ifdef _WIN32
    if (attached)
        ::AttachThreadInput(fgThread, myThread, FALSE);
#endif

    delete menu;

    // Restart timers that were running before the popup.
    if (sprintWasRunning)   m_sprintUpdateTimer->Start(SPRINT_UPDATE_INTERVAL_MS);
    if (configWasRunning)   m_configWatchTimer->Start(10000);


    m_menuShowing = false;
}

wxMenu* SprintToolBoxApp::BuildPopupMenu() {
    // Update timestamps before showing menu
    UpdateTimestamps();
    
    wxMenu* menu = new wxMenu();
    
    // Clear previous URL mappings
    m_menuUrlMap.clear();
    
    // Timestamp menu items (configurable)
    bool hasTimestampItems = false;
    if (m_config->GetShowUnixTimestamp()) {
        menu->Append(ID_COPY_UNIX, "Unix: " + m_unixTimestamp);
        hasTimestampItems = true;
    }
    if (m_config->GetShowZuluTimestamp()) {
        menu->Append(ID_COPY_ZULU, "Zulu: " + m_zuluTimestamp);
        hasTimestampItems = true;
    }
    if (m_config->GetShowTimeConverter()) {
        menu->Append(ID_OPEN_TIME_CONVERTER, "More...");
        hasTimestampItems = true;
    }

    // Converters (configurable)
    bool hasConverters = false;
    if (m_config->GetShowHexDecConverter()) {
        if (hasTimestampItems) menu->AppendSeparator();
        menu->Append(ID_OPEN_CONVERTER, "Hex/Dec Converter");
        hasConverters = true;
    }
    if (m_config->GetShowBase64Encoder()) {
        if (!hasConverters && hasTimestampItems) menu->AppendSeparator();
        menu->Append(ID_OPEN_BASE64_ENCODER, "Base64 Encoder");
        hasConverters = true;
    }
    if (m_config->GetShowUrlEncoder()) {
        if (!hasConverters && hasTimestampItems) menu->AppendSeparator();
        menu->Append(ID_OPEN_URL_ENCODER, "URL Encoder");
        hasConverters = true;
    }
    if (m_config->GetShowJsonFormatter()) {
        if (!hasConverters && hasTimestampItems) menu->AppendSeparator();
        menu->Append(ID_OPEN_JSON_FORMATTER, "JSON Formatter");
        hasConverters = true;
    }

    // Add dynamic menu items from config
    std::vector<MenuItem> menuItems = m_config->GetMainMenuItems();
    if (!menuItems.empty()) {
        if (hasTimestampItems || hasConverters) menu->AppendSeparator();
        
        int dynamicId = ID_DYNAMIC_MENU_START;
        for (const auto& item : menuItems) {
            if (!item.enabled) continue;
            if (item.isSeparator) {
                menu->AppendSeparator();
            } else if (item.url.StartsWith("submenu:")) {
                // TODO: Implement submenu support later
                wxString submenuName = item.url.Mid(8);
                wxMenu* submenu = new wxMenu();
                
                std::map<wxString, std::vector<MenuItem>> subMenus = m_config->GetSubMenus();
                if (subMenus.count(submenuName) > 0) {
                    for (const auto& subItem : subMenus[submenuName]) {
                        if (subItem.isSeparator) {
                            submenu->AppendSeparator();
                        } else {
                            submenu->Append(dynamicId, subItem.name);
                            m_menuUrlMap[dynamicId] = subItem.url;
                            dynamicId++;
                        }
                    }
                }
                menu->AppendSubMenu(submenu, item.name);
            } else {
                // Regular menu item with URL
                menu->Append(dynamicId, item.name);
                m_menuUrlMap[dynamicId] = item.url;
                dynamicId++;
            }
        }
    }
    
    menu->AppendSeparator();

    menu->Append(ID_CUSTOMIZE_MENU, "Customize Menu...");

    menu->AppendSeparator();

    wxMenuItem* autostartItem = menu->AppendCheckItem(ID_TOGGLE_AUTOSTART, "Start at login");
    autostartItem->Check(IsAutostartEnabled());

    if (!m_pendingUpdateVersion.IsEmpty()) {
        menu->Append(ID_CHECK_UPDATES,
            wxString::Format("v%s available – Download", m_pendingUpdateVersion));
    } else {
        menu->Append(ID_CHECK_UPDATES, "Check for updates...");
    }

    menu->AppendSeparator();
    wxMenuItem* versionItem = menu->Append(wxID_ANY,
        wxString::Format("Version %s", APP_VERSION));
    versionItem->Enable(false);

    menu->Append(ID_QUIT, "Quit");

    return menu;
}

void SprintToolBoxApp::OnCopyUnixTimestamp(wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_unixTimestamp));
        wxTheClipboard->Close();
    }
}

void SprintToolBoxApp::OnCopyZuluTimestamp(wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_zuluTimestamp));
        wxTheClipboard->Close();
    }
}

void SprintToolBoxApp::OnOpenConverter(wxCommandEvent& event) {
    if (!m_converterDialog) {
        m_converterDialog = new ConverterDialog(nullptr);
    }
    
    m_converterDialog->Show();
    m_converterDialog->Raise();
}

void SprintToolBoxApp::OnOpenTimeConverter(wxCommandEvent& event) {
    if (!m_timeConverterDialog) {
        m_timeConverterDialog = new TimeConverterDialog(nullptr);
    }

    m_timeConverterDialog->Show();
    m_timeConverterDialog->Raise();
    m_timeConverterDialog->ResetToCurrentTime();
}

void SprintToolBoxApp::OnOpenBase64Encoder(wxCommandEvent& event) {
    if (!m_base64Dialog) {
        m_base64Dialog = new Base64Dialog(nullptr);
    }

    m_base64Dialog->Show();
    m_base64Dialog->Raise();
}

void SprintToolBoxApp::OnOpenUrlEncoder(wxCommandEvent& event) {
    if (!m_urlEncoderDialog) {
        m_urlEncoderDialog = new UrlEncoderDialog(nullptr);
    }

    m_urlEncoderDialog->Show();
    m_urlEncoderDialog->Raise();
}

void SprintToolBoxApp::OnOpenJsonFormatter(wxCommandEvent& event) {
    if (!m_jsonFormatterDialog) {
        m_jsonFormatterDialog = new JsonFormatterDialog(nullptr);
    }

    m_jsonFormatterDialog->Show();
    m_jsonFormatterDialog->Raise();
}

void SprintToolBoxApp::OnDynamicMenuClick(wxCommandEvent& event) {
    int id = event.GetId();
    
    // Look up the URL for this menu item
    if (m_menuUrlMap.count(id) > 0) {
        wxString url = m_menuUrlMap[id];
        wxLogMessage("Opening URL: %s", url);
        
        // Open URL in default browser
        wxLaunchDefaultBrowser(url);
    } else {
        wxLogWarning("No URL found for menu item ID: %d", id);
    }
}

void SprintToolBoxApp::OnQuit(wxCommandEvent& event) {
#ifdef __WXOSX__
    RemoveIcon();
    // Defer NSStatusItem removal and wxExit so they run after
    // popUpMenuPositioningItem's modal loop ends.  Removing the status item
    // while its menu is still displayed crashes, and [NSApp terminate:nil]
    // is silently dropped inside a modal event loop.
    CallAfter([this]() {
        if (m_statusItem) {
            NSStatusItem* item = (__bridge NSStatusItem*)m_statusItem;
            [[NSStatusBar systemStatusBar] removeStatusItem:item];
            (void)CFBridgingRelease(m_statusItem);
            m_statusItem = nullptr;
        }
        if (m_statusItemHandler) {
            (void)CFBridgingRelease(m_statusItemHandler);
            m_statusItemHandler = nullptr;
        }
        _exit(0);
    });
#else
    RemoveIcon();
    _exit(0);
#endif
}

bool SprintToolBoxApp::IsAutostartEnabled() {
#ifdef __WXOSX__
    if (@available(macOS 13.0, *)) {
        return [SMAppService mainAppService].status == SMAppServiceStatusEnabled;
    }
    NSString* plistPath = [[NSHomeDirectory()
        stringByAppendingPathComponent:@"Library/LaunchAgents"]
        stringByAppendingPathComponent:@"com.sprinttoolbox.plist"];
    return [[NSFileManager defaultManager] fileExistsAtPath:plistPath];
#elif defined(_WIN32)
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD size = 0;
    bool found = RegQueryValueExW(key, L"SprintToolBox", nullptr,
                                  nullptr, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return found;
#else
    return wxFileExists(wxGetHomeDir() + "/.config/autostart/sprinttoolbox.desktop");
#endif
}

void SprintToolBoxApp::SetAutostart(bool enable) {
#ifdef __WXOSX__
    if (@available(macOS 13.0, *)) {
        NSError* err = nil;
        if (enable)
            [[SMAppService mainAppService] registerAndReturnError:&err];
        else
            [[SMAppService mainAppService] unregisterAndReturnError:&err];
        return;
    }
    NSString* laDir = [NSHomeDirectory()
        stringByAppendingPathComponent:@"Library/LaunchAgents"];
    NSString* plistPath = [laDir
        stringByAppendingPathComponent:@"com.sprinttoolbox.plist"];
    if (enable) {
        NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
        NSDictionary* plist = @{
            @"Label": @"com.sprinttoolbox",
            @"ProgramArguments": @[@"/usr/bin/open", @"-a", bundlePath],
            @"RunAtLoad": @YES,
            @"KeepAlive": @NO,
            @"ProcessType": @"Interactive"
        };
        NSError* err = nil;
        [[NSFileManager defaultManager] createDirectoryAtPath:laDir
            withIntermediateDirectories:YES attributes:nil error:&err];
        NSData* data = [NSPropertyListSerialization
            dataWithPropertyList:plist
            format:NSPropertyListXMLFormat_v1_0
            options:0 error:&err];
        if (data)
            [data writeToFile:plistPath options:NSDataWritingAtomic error:&err];
    } else {
        [[NSFileManager defaultManager] removeItemAtPath:plistPath error:nil];
    }
#elif defined(_WIN32)
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enable) {
        wxString exePath = wxStandardPaths::Get().GetExecutablePath();
        wxString quoted = wxString("\"") + exePath + wxString("\"");
        std::wstring wval = quoted.ToStdWstring();
        RegSetValueExW(key, L"SprintToolBox", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(wval.c_str()),
            static_cast<DWORD>((wval.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"SprintToolBox");
    }
    RegCloseKey(key);
#else
    wxString autoDir = wxGetHomeDir() + "/.config/autostart";
    wxString desktopPath = autoDir + "/sprinttoolbox.desktop";
    if (enable) {
        wxFileName::Mkdir(autoDir, 0755, wxPATH_MKDIR_FULL);
        wxFile f(desktopPath, wxFile::write);
        if (f.IsOpened()) {
            wxString exe = wxStandardPaths::Get().GetExecutablePath();
            f.Write(wxString::Format(
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=SprintToolBox\n"
                "Comment=Sprint tracking tray utility\n"
                "Exec=%s\n"
                "Icon=utilities-system-monitor\n"
                "StartupNotify=false\n"
                "X-GNOME-Autostart-enabled=true\n",
                exe));
        }
    } else {
        wxRemoveFile(desktopPath);
    }
#endif
}

void SprintToolBoxApp::OnToggleAutostart(wxCommandEvent& event) {
    SetAutostart(!IsAutostartEnabled());
}

void SprintToolBoxApp::OnCustomizeMenu(wxCommandEvent& event) {
    std::vector<MenuItem> items = m_config->GetMainMenuItems();
    std::map<wxString, std::vector<MenuItem>> subMenus = m_config->GetSubMenus();
    DisplayFlags flags = m_config->GetDisplayFlags();
    CallAfter([this, items, subMenus, flags]() {
        // Re-initialise with current state each time it's opened.
        if (m_customizeMenuDialog) {
            m_customizeMenuDialog->Destroy();
            m_customizeMenuDialog = nullptr;
        }
        m_customizeMenuDialog = new CustomizeMenuDialog(nullptr, items, subMenus, flags);
        if (m_customizeMenuDialog->ShowModal() == wxID_OK) {
            m_config->SaveMenuOrder(m_customizeMenuDialog->GetReorderedItems());
            m_config->SaveDisplayFlags(m_customizeMenuDialog->GetDisplayFlags());
            for (const auto& pair : m_customizeMenuDialog->GetModifiedSubMenus())
                m_config->SaveSubmenuOrder(pair.first, pair.second);
        }
        m_customizeMenuDialog->Destroy();
        m_customizeMenuDialog = nullptr;
    });
}

void SprintToolBoxApp::OnSprintUpdateTimer(wxTimerEvent& event) {
    wxLogMessage("Scheduled update timer fired.");
    UpdateSprint();
}

void SprintToolBoxApp::OnRetryTimer(wxTimerEvent& event) {
    wxLogMessage("Retrying sprint fetch after failure.");
    FetchPublicSprint();
}

void SprintToolBoxApp::OnConfigWatchTimer(wxTimerEvent& event) {
    if (Config::GetInstance().HasConfigFileChanged()) {
        wxLogMessage("Config file changed on disk – reloading.");
        Config::GetInstance().Reload();
        UpdateSprint();
    }
}

void SprintToolBoxApp::UpdateSprint() {
    FetchPublicSprint();
}

void SprintToolBoxApp::OnSprintFetched(const SprintInfo& sprint) {
    wxLogMessage("Sprint fetched: %s", sprint.name);
    m_retryTimer->Stop();

    wxString displayText = sprint.GetDisplayText();
    int daysPassed = sprint.GetDaysPassed();

    UpdateTrayIcon(displayText, daysPassed);
}

void SprintToolBoxApp::DownloadAndLaunch(const ReleaseInfo& info) {
    if (!info.HasAsset()) {
        // No matching platform asset — release page in browser is the best we can do.
        wxLaunchDefaultBrowser(info.htmlUrl);
        return;
    }

    auto* progressDlg = new wxProgressDialog(
        "Downloading update",
        wxString::Format("Downloading %s...", info.assetName),
        100, nullptr,
        wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH);
    progressDlg->Show();

    std::thread([this, info, progressDlg]() {
        wxFileName downloadedPath;
        wxString errorMsg;
        try {
            downloadedPath = m_updateService->DownloadAsset(info,
                [progressDlg](double fraction) {
                    int pct = static_cast<int>(fraction * 100.0);
                    if (pct < 0)  pct = 0;
                    if (pct > 99) pct = 99;
                    wxTheApp->CallAfter([progressDlg, pct]() {
                        if (progressDlg) progressDlg->Update(pct);
                    });
                });
        } catch (const std::exception& e) {
            errorMsg = wxString::FromUTF8(e.what());
        } catch (...) {
            errorMsg = "Unknown download error";
        }

        wxTheApp->CallAfter([this, progressDlg, downloadedPath, errorMsg]() {
            progressDlg->Update(100); // triggers wxPD_AUTO_HIDE
            progressDlg->Destroy();
            if (!errorMsg.IsEmpty()) {
                wxMessageBox(
                    wxString::Format("Download failed:\n\n%s", errorMsg),
                    "Update", wxOK | wxICON_ERROR);
                return;
            }
            LaunchDownloadedAsset(downloadedPath);
        });
    }).detach();
}

void SprintToolBoxApp::LaunchDownloadedAsset(const wxFileName& path) {
    const wxString fullPath = path.GetFullPath();
#if defined(__linux__)
    // AppImage assets must be executable before the desktop can run them.
    if (fullPath.EndsWith(".AppImage")) {
        ::chmod(fullPath.ToUTF8(), 0755);
    }
#endif
    if (!wxLaunchDefaultApplication(fullPath)) {
        wxMessageBox(
            wxString::Format("Could not open the downloaded file:\n%s", fullPath),
            "Update", wxOK | wxICON_ERROR);
    }
}

void SprintToolBoxApp::OnUpdateCheckTimer(wxTimerEvent& event) {
    CheckForUpdates(/*silent=*/true);
    // Reschedule for the next interval.
    if (m_updateCheckTimer) {
        m_updateCheckTimer->Start(UPDATE_CHECK_INTERVAL_MS, wxTIMER_ONE_SHOT);
    }
}

void SprintToolBoxApp::OnCheckUpdatesMenu(wxCommandEvent& event) {
    CheckForUpdates(/*silent=*/false);
}

void SprintToolBoxApp::CheckForUpdates(bool silent) {
    if (!m_updateService) return;
    wxLogMessage("Update check requested (silent=%d).", silent ? 1 : 0);

    // Run the HTTP request off the UI thread; marshal the result back via
    // CallAfter so the dialog is shown on the main thread.
    std::thread([this, silent]() {
        m_updateService->SetSuccessCallback(
            [this, silent](const ReleaseInfo& info) {
                CallAfter([this, info, silent]() {
                    OnUpdateAvailable(info, silent);
                });
            });
        m_updateService->SetErrorCallback(
            [this, silent](const wxString& msg, const wxString& code) {
                CallAfter([this, msg, code, silent]() {
                    OnUpdateError(msg, code, silent);
                });
            });
        m_updateService->CheckForUpdates();
    }).detach();
}

void SprintToolBoxApp::OnUpdateAvailable(const ReleaseInfo& info, bool silent) {
    const bool isNewer = UpdateService::IsNewerThanCurrent(info.tag);
    if (!isNewer) {
        if (!m_pendingUpdateVersion.IsEmpty()) {
            m_pendingUpdateVersion.Clear();
            UpdateTrayIcon(m_currentIconText, m_currentDaysPassed);
        }
        if (!silent) {
            wxMessageBox(
                wxString::Format("SprintToolBox %s is the latest version.", APP_VERSION),
                "No updates available", wxOK | wxICON_INFORMATION);
        }
        return;
    }

    if (m_config && m_config->GetSkippedVersion() == info.version) {
        wxLogMessage("Update %s suppressed (user skipped this version).", info.version);
        return;
    }

    if (silent) {
        if (m_pendingUpdateVersion != info.version) {
            m_pendingUpdateVersion = info.version;
            UpdateTrayIcon(m_currentIconText, m_currentDaysPassed);
            wxLogMessage("Update %s available; flagged in tray tooltip.", info.version);
        }
        return;
    }

    wxString notes = info.body;
    if (notes.length() > 500) notes = notes.Left(500) + "...";

    wxString message = wxString::Format(
        "SprintToolBox %s is available.\nYou have %s.",
        info.version, APP_VERSION);
    if (!notes.IsEmpty()) {
        message += "\n\nRelease notes:\n" + notes;
    }

    wxMessageDialog dlg(nullptr, message, "Update available",
                        wxYES_NO | wxCANCEL | wxICON_INFORMATION);
    dlg.SetYesNoCancelLabels("Download...", "Later", "Skip this version");

    const int result = dlg.ShowModal();
    if (result == wxID_YES) {
        DownloadAndLaunch(info);
    } else if (result == wxID_CANCEL) {
        if (m_config) m_config->SetSkippedVersion(info.version);
        m_pendingUpdateVersion.Clear();
        UpdateTrayIcon(m_currentIconText, m_currentDaysPassed);
        wxLogMessage("User skipped update %s.", info.version);
    }
}

void SprintToolBoxApp::OnUpdateError(const wxString& error,
                                     const wxString& code, bool silent) {
    wxLogWarning("Update check failed: %s (%s)", error, code);
    if (silent) return;
    wxString msg = error;
    if (error.Contains("403"))
        msg += "\n\nThis is most likely because the GitHub API rate limit has been"
               " reached on the free tier. Try again later.";
    wxMessageBox(wxString::Format("Could not check for updates:\n\n%s", msg),
                 "Update check failed", wxOK | wxICON_WARNING);
}

void SprintToolBoxApp::FetchPublicSprint() {
    // Hardcoded public sprint URL with cache-busting timestamp
    wxDateTime now = wxDateTime::Now();
    wxString publicURL = wxString::Format(
        "https://storage.googleapis.com/stb-current-sprint/current-sprint.json?t=%lld",
        now.GetValue().GetValue() / 1000
    );
    
    wxLogMessage("Fetching sprint from public source: %s", publicURL);
    
    // Fetch JSON from public URL using CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        wxLogError("Failed to initialize CURL for public sprint fetch");
        return;
    }
    
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, publicURL.utf8_str().data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::string* str = static_cast<std::string*>(userdata);
        str->append(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    // For the public fallback URL (GitHub CDN), disable SSL verification
    // to avoid CA certificate issues on portable Windows builds
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        wxLogError("Failed to fetch public sprint: %s", curl_easy_strerror(res));
        UpdateTrayIcon("!");
        m_retryTimer->Start(NETWORK_RETRY_INTERVAL_MS, wxTIMER_ONE_SHOT);
        return;
    }
    
    // Parse using JiraService helper
    wxString json(response);
    SprintInfo sprint = JiraService::ParsePublicSprintJson(json);
    
    if (sprint.name == "Unknown Sprint" || sprint.name.IsEmpty()) {
        wxLogError("Failed to parse sprint from public source");
        UpdateTrayIcon("!");
        m_retryTimer->Start(NETWORK_RETRY_INTERVAL_MS, wxTIMER_ONE_SHOT);
        return;
    }
    
    // Display the sprint
    OnSprintFetched(sprint);
    wxLogMessage("Successfully loaded sprint from public source: %s", sprint.name);
}
