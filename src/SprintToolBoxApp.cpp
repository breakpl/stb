#include "SprintToolBoxApp.h"
#include "ConverterDialog.h"
#include "TimeConverterDialog.h"
#include "JiraService.h"
#include "Config.h"
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/icon.h>
#include <wx/bitmap.h>

#ifdef _WIN32
#include <windows.h>
#include <string>
#endif

#ifdef __WXOSX__
#include <wx/osx/private.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

// Helper to observe theme changes
@interface ThemeChangeObserver : NSObject
@property (assign) SprintToolBoxApp* owner;
@end

@implementation ThemeChangeObserver
- (void)themeChanged:(NSNotification*)notification {
    if (self.owner) {
        // Trigger icon update when theme changes
        self.owner->UpdateTrayIcon(self.owner->m_currentIconText, self.owner->m_currentDaysPassed);
    }
}
@end

#endif

// Constants for menu bar icon
static const int ICON_FONT_SIZE_MAC = 14;  // SF Mono Regular – sized for attributedTitle
static const int SPRINT_UPDATE_INTERVAL_MS  = 300000; // 5 minutes
static const int NETWORK_RETRY_INTERVAL_MS  = 20000;  // 20 s  (window: 2 min)
static const int NETWORK_RETRY_MAX_COUNT    = 6;      // 6 × 20 s = 2 min
static const int AUTH_RETRY_INTERVAL_MS     = 30000;  // 30 s  (window: 5 min)
static const int AUTH_RETRY_MAX_COUNT       = 10;     // 10 × 30 s = 5 min

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
    EVT_MENU(ID_QUIT, SprintToolBoxApp::OnQuit)
    EVT_TIMER(ID_SPRINT_TIMER, SprintToolBoxApp::OnSprintUpdateTimer)
    EVT_TIMER(ID_RETRY_TIMER,  SprintToolBoxApp::OnRetryTimer)
    EVT_TIMER(ID_CONFIG_WATCH_TIMER, SprintToolBoxApp::OnConfigWatchTimer)
    EVT_MENU_RANGE(ID_DYNAMIC_MENU_START, ID_DYNAMIC_MENU_START + 999, SprintToolBoxApp::OnDynamicMenuClick)
    EVT_TASKBAR_LEFT_UP(SprintToolBoxApp::OnTaskBarClick)
    EVT_TASKBAR_RIGHT_UP(SprintToolBoxApp::OnTaskBarClick)
wxEND_EVENT_TABLE()

SprintToolBoxApp::SprintToolBoxApp() 
    : wxTaskBarIcon()
    , m_converterDialog(nullptr)
    , m_timeConverterDialog(nullptr)
    , m_jiraService(nullptr)
    , m_config(nullptr)
    , m_sprintUpdateTimer(nullptr)
    , m_configWatchTimer(nullptr)
    , m_retryTimer(nullptr)
    , m_retryCount(0)
    , m_retryMaxCount(0)
    , m_currentIconText("...")
    , m_currentDaysPassed(-1)
#ifdef __WXOSX__
    , m_themeObserver(nullptr)
    , m_statusItem(nullptr)
#endif
#ifdef _WIN32
    , m_themeHwnd(nullptr)
#endif
{
    // Initialize configuration
    m_config = & Config::GetInstance();
    
    // Initialize JIRA service
    m_jiraService = new JiraService();
    m_jiraService->SetSuccessCallback([this](const SprintInfo& sprint) {
        OnSprintFetched(sprint);
    });
    m_jiraService->SetErrorCallback([this](const wxString& error, const wxString& code) {
        OnSprintError(error, code);
    });
    
    // Setup timer for periodic sprint updates
    m_sprintUpdateTimer = new wxTimer(this, ID_SPRINT_TIMER);
    m_sprintUpdateTimer->Start(SPRINT_UPDATE_INTERVAL_MS);

    // Retry timer (started on demand when a transient error occurs)
    m_retryTimer = new wxTimer(this, ID_RETRY_TIMER);

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
        // Don't remove the status item, just release our reference to it
        // wxTaskBarIcon will handle removing it
        (void)CFBridgingRelease(m_statusItem);
        m_statusItem = nullptr;
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

    if (m_retryTimer) {
        m_retryTimer->Stop();
        delete m_retryTimer;
        m_retryTimer = nullptr;
    }
    
    if (m_jiraService) {
        delete m_jiraService;
        m_jiraService = nullptr;
    }
    
    if (m_converterDialog) {
        m_converterDialog->Destroy();
        m_converterDialog = nullptr;
    }

    if (m_timeConverterDialog) {
        m_timeConverterDialog->Destroy();
        m_timeConverterDialog = nullptr;
    }
}

void SprintToolBoxApp::UpdateTrayIcon(const wxString& text, int daysPassed) {
    // Store current values for theme change updates
    m_currentIconText = text;
    m_currentDaysPassed = daysPassed;
    
#ifdef __WXOSX__
    // On macOS, use NSStatusItem's attributedTitle for native text rendering
    // (like Qt does) instead of drawing text into a bitmap image.
    // This gives automatic dark/light mode adaptation and proper font rendering.
    @autoreleasepool {
        // Create tooltip with sprint info
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) {
            tooltip += wxString::Format(" (Day %d)", daysPassed);
        }
        
        // On first call, we need SetIcon to let wxTaskBarIcon create its NSStatusItem
        if (!m_statusItem) {
            // Create a tiny transparent image just to bootstrap the status item
            NSImage* dummyImage = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
            [dummyImage setTemplate:YES];
            wxBitmap bitmap;
            WXImage wxImg = (WXImage)dummyImage;
            bitmap = wxBitmap(wxImg);
            wxIcon icon;
            icon.CopyFromBitmap(bitmap);
            SetIcon(icon, tooltip);
            
            // Find our status item using KVC (statusItems is a private API)
            NSString* nsTooltip = [NSString stringWithUTF8String:tooltip.utf8_str()];
            NSArray* items = [[NSStatusBar systemStatusBar] valueForKey:@"statusItems"];
            for (NSStatusItem* item in items) {
                if (item.button && [item.button.toolTip isEqualToString:nsTooltip]) {
                    m_statusItem = (void*)CFBridgingRetain(item);
                    break;
                }
            }
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
    // Windows: render text into a tray icon with proper alpha transparency
    {
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) tooltip += wxString::Format(" (Day %d)", daysPassed);

        // Icon size (DPI-aware)
        int iconSize = ::GetSystemMetrics(SM_CXSMICON);
        if (iconSize < 16) iconSize = 16;

        // Detect dark/light taskbar via registry
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

        // --- Step 1: Render WHITE text on BLACK background into a temp DIB ---
        // GDI ClearType/antialiasing will produce shades of grey which we use
        // as the coverage (alpha) mask.
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = iconSize;
        bmi.bmiHeader.biHeight      = -iconSize; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC screenDC = ::GetDC(nullptr);
        HBITMAP hBmp = ::CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC memDC = ::CreateCompatibleDC(screenDC);
        HBITMAP oldBmp = (HBITMAP)::SelectObject(memDC, hBmp);

        // Black background
        memset(bits, 0, iconSize * iconSize * 4);

        // Font: bold, sized to fit
        int textLen = (int)text.length();
        int fontSize;
        if (textLen <= 2)      fontSize = iconSize - 2;
        else if (textLen <= 3) fontSize = iconSize - 4;
        else                   fontSize = iconSize - 6;
        if (fontSize < 6) fontSize = 6;

        HFONT hFont = ::CreateFontW(
            -fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT oldFont = (HFONT)::SelectObject(memDC, hFont);

        // Draw WHITE text so any pixel brightness = coverage
        ::SetBkMode(memDC, TRANSPARENT);
        ::SetTextColor(memDC, RGB(255, 255, 255));

        RECT rc = { 0, 0, iconSize, iconSize };
        ::DrawTextW(memDC, text.wc_str(), -1, &rc,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);

        ::SelectObject(memDC, oldFont);
        ::DeleteObject(hFont);
        ::SelectObject(memDC, oldBmp);
        ::DeleteDC(memDC);
        ::ReleaseDC(nullptr, screenDC);

        // --- Step 2: Convert to premultiplied BGRA ---
        // For each pixel, the max of (R,G,B) written by GDI = coverage/alpha.
        // Then set the color channels to target color * alpha / 255.
        BYTE* pixel = (BYTE*)bits;
        for (int i = 0; i < iconSize * iconSize; i++, pixel += 4) {
            BYTE b = pixel[0], g = pixel[1], r = pixel[2];
            // Use max channel as alpha (handles ClearType sub-pixel variation)
            BYTE alpha = r;
            if (g > alpha) alpha = g;
            if (b > alpha) alpha = b;

            if (alpha > 0) {
                // Premultiplied alpha: channel = targetColor * alpha / 255
                pixel[0] = (BYTE)(fgB * alpha / 255); // B
                pixel[1] = (BYTE)(fgG * alpha / 255); // G
                pixel[2] = (BYTE)(fgR * alpha / 255); // R
                pixel[3] = alpha;                      // A
            }
            // else: stays (0,0,0,0) = fully transparent
        }

        // --- Step 3: Create HICON ---
        HBITMAP hMask = ::CreateBitmap(iconSize, iconSize, 1, 1, nullptr);
        {
            HDC maskDC = ::CreateCompatibleDC(nullptr);
            HBITMAP oldMask = (HBITMAP)::SelectObject(maskDC, hMask);
            RECT mr = { 0, 0, iconSize, iconSize };
            ::FillRect(maskDC, &mr, (HBRUSH)::GetStockObject(BLACK_BRUSH));
            ::SelectObject(maskDC, oldMask);
            ::DeleteDC(maskDC);
        }

        ICONINFO ii = {};
        ii.fIcon    = TRUE;
        ii.hbmMask  = hMask;
        ii.hbmColor = hBmp;
        HICON hIcon = ::CreateIconIndirect(&ii);

        ::DeleteObject(hMask);
        ::DeleteObject(hBmp);

        if (hIcon) {
            wxIcon icon;
            icon.CreateFromHICON((WXHICON)hIcon);
            m_trayIconCurrent = icon;
            SetIcon(m_trayIconCurrent, tooltip);
        }
    }
#else
    // Linux/generic fallback: simple wxWidgets bitmap icon
    {
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) tooltip += wxString::Format(" (Day %d)", daysPassed);

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
    // Return nullptr – we handle the popup manually via OnTaskBarClick()
    // so that we can (a) show the menu on left-click too, and (b) apply
    // the Windows SetForegroundWindow fix reliably (KB Q135788).
    return nullptr;
}

void SprintToolBoxApp::OnTaskBarClick(wxTaskBarIconEvent& WXUNUSED(event)) {
    ShowContextMenu();
}

void SprintToolBoxApp::ShowContextMenu() {
    wxMenu* menu = BuildPopupMenu();
    if (!menu) return;

#ifdef _WIN32
    // KB Q135788 – TrackPopupMenu requires the owner window to be the
    // foreground window.  wxTaskBarIcon::PopupMenu() calls
    // SetForegroundWindow internally, but that API can silently fail if
    // another thread/process owns the foreground lock.  Temporarily
    // attaching our thread input to the foreground thread lets
    // SetForegroundWindow succeed reliably.
    HWND fgWnd = ::GetForegroundWindow();
    DWORD fgThread = fgWnd ? ::GetWindowThreadProcessId(fgWnd, NULL) : 0;
    DWORD myThread = ::GetCurrentThreadId();
    bool attached = false;
    if (fgThread && fgThread != myThread) {
        attached = (::AttachThreadInput(fgThread, myThread, TRUE) != 0);
    }
#endif

    PopupMenu(menu);

#ifdef _WIN32
    if (attached) {
        ::AttachThreadInput(fgThread, myThread, FALSE);
    }
#endif

    delete menu;
}

wxMenu* SprintToolBoxApp::BuildPopupMenu() {
    // Update timestamps before showing menu
    UpdateTimestamps();
    
    wxMenu* menu = new wxMenu();
    
    // Clear previous URL mappings
    m_menuUrlMap.clear();
    
    // Timestamp menu items
    menu->Append(ID_COPY_UNIX, "Unix: " + m_unixTimestamp);
    menu->Append(ID_COPY_ZULU, "Zulu: " + m_zuluTimestamp);
    menu->Append(ID_OPEN_TIME_CONVERTER, "More...");

    menu->AppendSeparator();

    // Converters
    menu->Append(ID_OPEN_CONVERTER, "Hex/Dec Converter");

    // Add dynamic menu items from config
    std::vector<MenuItem> menuItems = m_config->GetMainMenuItems();
    if (!menuItems.empty()) {
        menu->AppendSeparator();
        
        int dynamicId = ID_DYNAMIC_MENU_START;
        for (const auto& item : menuItems) {
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
    
    // Quit
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
    RemoveIcon();
    wxExit();
}

void SprintToolBoxApp::OnSprintUpdateTimer(wxTimerEvent& event) {
    wxLogMessage("Scheduled update timer fired.");
    UpdateSprint();
}

void SprintToolBoxApp::OnRetryTimer(wxTimerEvent& event) {
    m_retryCount++;
    if (m_retryCount >= m_retryMaxCount) {
        m_retryTimer->Stop();
        m_retryCount = 0;
        wxLogWarning("Retry window exhausted, resuming normal schedule.");
        return;
    }
    wxLogMessage("Retry attempt %d/%d...", m_retryCount, m_retryMaxCount);
    // Reload credentials on each retry so an updated ini is picked up immediately
    if (m_jiraService) m_jiraService->ReloadCredentials();
    UpdateSprint();
}

void SprintToolBoxApp::OnConfigWatchTimer(wxTimerEvent& event) {
    if (Config::GetInstance().HasConfigFileChanged()) {
        wxLogMessage("Config file changed on disk – reloading.");
        UpdateSprint();
    }
}

void SprintToolBoxApp::UpdateSprint() {
    if (m_jiraService) {
        m_jiraService->ReloadCredentials(); // re-read ini on every tick
        m_jiraService->FetchCurrentSprint();
    }
}

void SprintToolBoxApp::OnSprintFetched(const SprintInfo& sprint) {
    // Clear any active error-retry loop
    if (m_retryTimer && m_retryTimer->IsRunning()) {
        m_retryTimer->Stop();
        m_retryCount = 0;
    }

    wxLogMessage("Sprint fetched: %s", sprint.name);
    
    wxString displayText = sprint.GetDisplayText();
    int daysPassed = sprint.GetDaysPassed();
    
    UpdateTrayIcon(displayText, daysPassed);
}

void SprintToolBoxApp::OnSprintError(const wxString& error, const wxString& errorCode) {
    wxLogWarning("Sprint fetch error: %s (%s)", error, errorCode);

    if (errorCode == "NETWORK_ERROR") {
#ifdef _WIN32
        UpdateTrayIcon("NE!");
#else
        UpdateTrayIcon("net.err");
#endif
        // Start network-error retry loop (every 20 s, up to 2 min)
        if (m_retryTimer && !m_retryTimer->IsRunning()) {
            m_retryCount    = 0;
            m_retryMaxCount = NETWORK_RETRY_MAX_COUNT;
            m_retryTimer->Start(NETWORK_RETRY_INTERVAL_MS);
        }
    } else if (errorCode == "AUTH_ERROR") {
#ifdef _WIN32
        UpdateTrayIcon("EA!");
#else
        UpdateTrayIcon("auth.err");
#endif
        // Start auth-error retry loop (every 30 s, up to 5 min)
        if (m_retryTimer && !m_retryTimer->IsRunning()) {
            m_retryCount    = 0;
            m_retryMaxCount = AUTH_RETRY_MAX_COUNT;
            m_retryTimer->Start(AUTH_RETRY_INTERVAL_MS);
        }
    } else {
        UpdateTrayIcon(errorCode);
    }
}
