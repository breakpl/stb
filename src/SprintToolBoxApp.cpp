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
static const int SPRINT_UPDATE_INTERVAL_MS = 300000; // 5 minutes

wxBEGIN_EVENT_TABLE(SprintToolBoxApp, wxTaskBarIcon)
    EVT_MENU(ID_COPY_UNIX, SprintToolBoxApp::OnCopyUnixTimestamp)
    EVT_MENU(ID_COPY_ZULU, SprintToolBoxApp::OnCopyZuluTimestamp)
    EVT_MENU(ID_OPEN_CONVERTER, SprintToolBoxApp::OnOpenConverter)
    EVT_MENU(ID_OPEN_TIME_CONVERTER, SprintToolBoxApp::OnOpenTimeConverter)
    EVT_MENU(ID_QUIT, SprintToolBoxApp::OnQuit)
    EVT_TIMER(ID_SPRINT_TIMER, SprintToolBoxApp::OnSprintUpdateTimer)
    EVT_MENU_RANGE(ID_DYNAMIC_MENU_START, ID_DYNAMIC_MENU_START + 999, SprintToolBoxApp::OnDynamicMenuClick)
wxEND_EVENT_TABLE()

SprintToolBoxApp::SprintToolBoxApp() 
    : wxTaskBarIcon()
    , m_converterDialog(nullptr)
    , m_timeConverterDialog(nullptr)
    , m_jiraService(nullptr)
    , m_config(nullptr)
    , m_sprintUpdateTimer(nullptr)
    , m_currentIconText("...")
    , m_currentDaysPassed(-1)
#ifdef __WXOSX__
    , m_themeObserver(nullptr)
    , m_statusItem(nullptr)
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
    
    // Initial icon and data
    UpdateTrayIcon("...");
    UpdateTimestamps();
    UpdateSprint();
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
    
    if (m_sprintUpdateTimer) {
        m_sprintUpdateTimer->Stop();
        delete m_sprintUpdateTimer;
        m_sprintUpdateTimer = nullptr;
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
#else
    // Windows: render ClearType text on an opaque background that matches
    // the taskbar colour — exactly how the system clock is drawn.
    int iconSize = ::GetSystemMetrics(SM_CXSMICON);
    if (iconSize <= 0) iconSize = 16;

    // Determine the exact taskbar background colour from Windows registry settings.
    // Handles: dark/light theme, accent colour on taskbar, and transparency.
    COLORREF bgColor = RGB(31, 31, 31); // dark theme default
    COLORREF textColor = RGB(255, 255, 255);
    {
        DWORD systemLight = 0, colorPrevalence = 0, transparency = 0;
        HKEY hPersonalize;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                0, KEY_READ, &hPersonalize) == ERROR_SUCCESS) {
            DWORD sz = sizeof(DWORD);
            RegQueryValueExW(hPersonalize, L"SystemUsesLightTheme", NULL, NULL, (LPBYTE)&systemLight, &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPersonalize, L"ColorPrevalence", NULL, NULL, (LPBYTE)&colorPrevalence, &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPersonalize, L"EnableTransparency", NULL, NULL, (LPBYTE)&transparency, &sz);
            RegCloseKey(hPersonalize);
        }

        if (systemLight) {
            // Light taskbar
            bgColor = RGB(243, 243, 243);
            textColor = RGB(0, 0, 0);
        } else if (colorPrevalence) {
            // Dark theme + accent colour on taskbar: read DWM accent
            HKEY hDwm;
            if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\DWM",
                    0, KEY_READ, &hDwm) == ERROR_SUCCESS) {
                DWORD abgr = 0, sz = sizeof(DWORD);
                if (RegQueryValueExW(hDwm, L"AccentColor", NULL, NULL, (LPBYTE)&abgr, &sz) == ERROR_SUCCESS) {
                    // AccentColor is stored as 0xAABBGGRR
                    bgColor = RGB(abgr & 0xFF, (abgr >> 8) & 0xFF, (abgr >> 16) & 0xFF);
                }
                RegCloseKey(hDwm);
            }
            // Light or dark text based on accent luminance
            int lum = (GetRValue(bgColor) * 299 + GetGValue(bgColor) * 587 + GetBValue(bgColor) * 114) / 1000;
            textColor = (lum > 128) ? RGB(0, 0, 0) : RGB(255, 255, 255);
        } else {
            // Dark theme, no accent → standard dark taskbar
            bgColor = RGB(31, 31, 31);
            textColor = RGB(255, 255, 255);
        }
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = iconSize;
    bmi.bmiHeader.biHeight      = -iconSize; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    DWORD* pixels = nullptr;
    HDC screenDC = ::GetDC(NULL);
    HBITMAP hBmp = ::CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    HDC memDC = ::CreateCompatibleDC(screenDC);
    HBITMAP oldBmp = (HBITMAP)::SelectObject(memDC, hBmp);

    // Fill with the exact taskbar background colour
    HBRUSH hBgBrush = ::CreateSolidBrush(bgColor);
    RECT fillRc = { 0, 0, iconSize, iconSize };
    ::FillRect(memDC, &fillRc, hBgBrush);
    ::DeleteObject(hBgBrush);

    // Draw crisp pixel-aliased text — at 16×16 ClearType just blurs things.
    // NONANTIALIASED_QUALITY gives sharp pixel edges like bitmap fonts.
    // Use pixel height directly for precise control at small sizes.
    int fh = (text.Length() <= 2) ? -(iconSize - 3) : -(iconSize - 5);
    HFONT hFont = ::CreateFontW(fh, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT oldFont = (HFONT)::SelectObject(memDC, hFont);
    ::SetBkMode(memDC, TRANSPARENT);
    ::SetTextColor(memDC, textColor);
    std::wstring wtext = text.ToStdWstring();
    RECT rc = { 0, 0, iconSize, iconSize };
    ::DrawTextW(memDC, wtext.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(memDC, oldFont);
    ::DeleteObject(hFont);

    // Set alpha=255 for all pixels (fully opaque icon, background blends naturally)
    for (int i = 0; i < iconSize * iconSize; i++)
        pixels[i] |= 0xFF000000;

    ::SelectObject(memDC, oldBmp);
    ::DeleteDC(memDC);

    // Mask: all zeros = use color bitmap always
    HBITMAP hMask = ::CreateBitmap(iconSize, iconSize, 1, 1, NULL);
    HDC maskDC = ::CreateCompatibleDC(NULL);
    HBITMAP oldMask = (HBITMAP)::SelectObject(maskDC, hMask);
    ::PatBlt(maskDC, 0, 0, iconSize, iconSize, BLACKNESS);
    ::SelectObject(maskDC, oldMask);
    ::DeleteDC(maskDC);

    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmMask  = hMask;
    ii.hbmColor = hBmp;
    HICON hIcon = ::CreateIconIndirect(&ii);

    ::DeleteObject(hBmp);
    ::DeleteObject(hMask);
    ::ReleaseDC(NULL, screenDC);

    wxIcon icon;
    icon.CreateFromHICON((WXHICON)hIcon);

    wxString tooltip = "Sprint " + text;
    if (daysPassed >= 0) {
        tooltip += wxString::Format(" (Day %d)", daysPassed);
    }
    SetIcon(icon, tooltip);
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
    UpdateSprint();
}

void SprintToolBoxApp::UpdateSprint() {
    if (m_jiraService) {
        m_jiraService->FetchCurrentSprint();
    }
}

void SprintToolBoxApp::OnSprintFetched(const SprintInfo& sprint) {
    wxLogMessage("Sprint fetched: %s", sprint.name);
    
    wxString displayText = sprint.GetDisplayText();
    int daysPassed = sprint.GetDaysPassed();
    
    UpdateTrayIcon(displayText, daysPassed);
}

void SprintToolBoxApp::OnSprintError(const wxString& error, const wxString& errorCode) {
    wxLogWarning("Sprint fetch error: %s", error);
    UpdateTrayIcon(errorCode);
    // Tooltip is already set in UpdateTrayIcon
}
