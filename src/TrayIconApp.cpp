#include "TrayIconApp.h"
#include "ConverterDialog.h"
#include "JiraService.h"
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
#endif

// Constants matching Qt version
static const int ICON_WIDTH = 100;
static const int ICON_HEIGHT = 44;
static const int ICON_FONT_SIZE_MAC = 46;
static const int SPRINT_UPDATE_INTERVAL_MS = 300000; // 5 minutes

wxBEGIN_EVENT_TABLE(TrayIconApp, wxTaskBarIcon)
    EVT_TIMER(ID_SPRINT_TIMER, TrayIconApp::OnSprintUpdateTimer)
wxEND_EVENT_TABLE()

TrayIconApp::TrayIconApp() 
    : wxTaskBarIcon()
    , m_converterDialog(nullptr)
    , m_jiraService(nullptr)
    , m_sprintUpdateTimer(nullptr)
{
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
    
    // Initial icon and data
    UpdateTrayIcon("...");
    UpdateTimestamps();
    UpdateSprint();
}

TrayIconApp::~TrayIconApp() {
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

void TrayIconApp::UpdateTrayIcon(const wxString& text, int daysPassed) {
#ifdef __WXOSX__
    // On macOS, create NSImage directly and set as template
    @autoreleasepool {
        NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(ICON_WIDTH, ICON_HEIGHT)];
        [image lockFocus];
        
        // Clear to transparent
        [[NSColor clearColor] set];
        NSRectFill(NSMakeRect(0, 0, ICON_WIDTH, ICON_HEIGHT));
        
        // Get SF Mono Bold font (Apple's monospace font)
        NSFont* font = [NSFont fontWithName:@"SFMono-Bold" size:ICON_FONT_SIZE_MAC];
        if (!font) {
            font = [NSFont fontWithName:@"SFMono-Regular" size:ICON_FONT_SIZE_MAC];
            if (font) {
                NSFontManager* fontManager = [NSFontManager sharedFontManager];
                font = [fontManager convertFont:font toHaveTrait:NSBoldFontMask];
            }
        }
        if (!font) {
            // Fallback: system bold font
            font = [NSFont boldSystemFontOfSize:ICON_FONT_SIZE_MAC];
        }
        
        // Set up attributes for text drawing with anti-aliasing
        NSMutableParagraphStyle* paragraphStyle = [[NSMutableParagraphStyle alloc] init];
        [paragraphStyle setAlignment:NSTextAlignmentCenter];
        
        NSDictionary* attributes = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: [NSColor blackColor],
            NSParagraphStyleAttributeName: paragraphStyle
        };
        
        NSString* nsText = [NSString stringWithUTF8String:text.utf8_str()];
        NSSize textSize = [nsText sizeWithAttributes:attributes];
        
        // Center the text with slight downward adjustment (matching Qt)
        CGFloat x = (ICON_WIDTH - textSize.width) / 2.0;
        CGFloat y = (ICON_HEIGHT - textSize.height) / 2.0 + (ICON_HEIGHT * 0.05);
        
        [nsText drawAtPoint:NSMakePoint(x, y) withAttributes:attributes];
        
        [image unlockFocus];
        
        // Set as template to enable automatic mode theming
        [image setTemplate:YES];
        
        // Convert NSImage to wxBitmap
        wxBitmap bitmap;
        WXImage wxImage = (WXImage)image;
        bitmap = wxBitmap(wxImage);
        
        wxIcon icon;
        icon.CopyFromBitmap(bitmap);
        
        // Create tooltip with sprint info
        wxString tooltip = "Sprint " + text;
        if (daysPassed >= 0) {
            tooltip += wxString::Format(" (Day %d)", daysPassed);
        }
        SetIcon(icon, tooltip);
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
        wxFont font(wxFontInfo(ICON_FONT_SIZE_MAC).FaceName("Monaco").Bold());
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

void TrayIconApp::UpdateTimestamps() {
    wxDateTime now = wxDateTime::Now();
    
    // Unix timestamp
    m_unixTimestamp = wxString::Format("%lld", now.GetValue().GetValue() / 1000);
    
    // Zulu (ISO 8601) timestamp
    wxDateTime utcNow = now.ToUTC();
    m_zuluTimestamp = utcNow.FormatISOCombined('T') + "Z";
}

wxMenu* TrayIconApp::CreatePopupMenu() {
    // Update timestamps before showing menu
    UpdateTimestamps();
    
    wxMenu* menu = new wxMenu();
    
    // Timestamp menu items
    menu->Append(ID_COPY_UNIX, "Unix: " + m_unixTimestamp);
    menu->Append(ID_COPY_ZULU, "Zulu: " + m_zuluTimestamp);
    
    menu->AppendSeparator();
    
    // Converter
    menu->Append(ID_OPEN_CONVERTER, "Hex/Dec Converter");
    
    menu->AppendSeparator();
    
    // Quit
    menu->Append(ID_QUIT, "Quit");
    
    // Bind events
    menu->Bind(wxEVT_MENU, &TrayIconApp::OnCopyUnixTimestamp, this, ID_COPY_UNIX);
    menu->Bind(wxEVT_MENU, &TrayIconApp::OnCopyZuluTimestamp, this, ID_COPY_ZULU);
    menu->Bind(wxEVT_MENU, &TrayIconApp::OnOpenConverter, this, ID_OPEN_CONVERTER);
    menu->Bind(wxEVT_MENU, &TrayIconApp::OnQuit, this, ID_QUIT);
    
    return menu;
}

void TrayIconApp::OnCopyUnixTimestamp(wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_unixTimestamp));
        wxTheClipboard->Close();
    }
}

void TrayIconApp::OnCopyZuluTimestamp(wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_zuluTimestamp));
        wxTheClipboard->Close();
    }
}

void TrayIconApp::OnOpenConverter(wxCommandEvent& event) {
    if (!m_converterDialog) {
        m_converterDialog = new ConverterDialog(nullptr);
    }
    
    m_converterDialog->Show();
    m_converterDialog->Raise();
}

void TrayIconApp::OnQuit(wxCommandEvent& event) {
    RemoveIcon();
    wxExit();
}

void TrayIconApp::OnUpdateTimestamps(wxCommandEvent& event) {
    UpdateTimestamps();
}

void TrayIconApp::OnMenuOpen(wxMenuEvent& event) {
    UpdateTimestamps();
}

void TrayIconApp::OnSprintUpdateTimer(wxTimerEvent& event) {
    UpdateSprint();
}

void TrayIconApp::UpdateSprint() {
    if (m_jiraService) {
        m_jiraService->FetchCurrentSprint();
    }
}

void TrayIconApp::OnSprintFetched(const SprintInfo& sprint) {
    wxLogMessage("Sprint fetched: %s", sprint.name);
    
    wxString displayText = sprint.GetDisplayText();
    int daysPassed = sprint.GetDaysPassed();
    
    UpdateTrayIcon(displayText, daysPassed);
}

void TrayIconApp::OnSprintError(const wxString& error, const wxString& errorCode) {
    wxLogWarning("Sprint fetch error: %s", error);
    UpdateTrayIcon(errorCode);
    SetIcon(GetIcon(), error); // Update tooltip with error message
}
