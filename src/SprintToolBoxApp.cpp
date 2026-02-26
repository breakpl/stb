#include "SprintToolBoxApp.h"
#include "ConverterDialog.h"
#include "JiraService.h"
#include "Config.h"
#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/icon.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/rawbmp.h>

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
static const int ICON_WIDTH = 36;
static const int ICON_HEIGHT = 22;
static const int ICON_FONT_SIZE_MAC = 14;  // SF Mono Regular – sized for attributedTitle
static const int SPRINT_UPDATE_INTERVAL_MS = 300000; // 5 minutes

wxBEGIN_EVENT_TABLE(SprintToolBoxApp, wxTaskBarIcon)
    EVT_MENU(ID_COPY_UNIX, SprintToolBoxApp::OnCopyUnixTimestamp)
    EVT_MENU(ID_COPY_ZULU, SprintToolBoxApp::OnCopyZuluTimestamp)
    EVT_MENU(ID_OPEN_CONVERTER, SprintToolBoxApp::OnOpenConverter)
    EVT_MENU(ID_QUIT, SprintToolBoxApp::OnQuit)
    EVT_TIMER(ID_SPRINT_TIMER, SprintToolBoxApp::OnSprintUpdateTimer)
    EVT_MENU_RANGE(ID_DYNAMIC_MENU_START, ID_DYNAMIC_MENU_START + 999, SprintToolBoxApp::OnDynamicMenuClick)
wxEND_EVENT_TABLE()

SprintToolBoxApp::SprintToolBoxApp() 
    : wxTaskBarIcon()
    , m_converterDialog(nullptr)
    , m_jiraService(nullptr)
    , m_config(nullptr)
    , m_sprintUpdateTimer(nullptr)
    , m_currentIconText("...")
    , m_currentDaysPassed(-1)
    , m_themeObserver(nullptr)
    , m_statusItem(nullptr)
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
    // Fallback for other platforms
    wxBitmap bitmap(ICON_WIDTH, ICON_HEIGHT, 32);
    wxMemoryDC memDC;
    memDC.SelectObject(bitmap);
    memDC.SetBackground(*wxTRANSPARENT_BRUSH);
    memDC.Clear();
    
    wxGraphicsContext* gc = wxGraphicsContext::Create(memDC);
    if (gc) {
        wxFont font(wxFontInfo(ICON_FONT_SIZE_MAC).FaceName("SFMono").Bold());
        gc->SetFont(font, *wxBLACK);
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        
        double textWidth, textHeight;
        gc->GetTextExtent(text, &textWidth, &textHeight);
        
        double x = (ICON_WIDTH - textWidth) / 2.0;
        double y = (ICON_HEIGHT - textHeight) / 2.0 + (ICON_HEIGHT * 0.05);
        
        gc->DrawText(text, x, y);
        delete gc;
    }
    
    memDC.SelectObject(wxNullBitmap);
    
    wxIcon icon;
    icon.CopyFromBitmap(bitmap);
    
    // Create tooltip with sprint info
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
    
    menu->AppendSeparator();
    
    // Converter
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

void SprintToolBoxApp::OnUpdateTimestamps(wxCommandEvent& event) {
    UpdateTimestamps();
}

void SprintToolBoxApp::OnMenuOpen(wxMenuEvent& event) {
    UpdateTimestamps();
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
