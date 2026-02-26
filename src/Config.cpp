#include "Config.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

Config::Config() : m_jiraBoardID(0) {
    LoadConfig();
}

Config::~Config() {
}

void Config::Reload() {
    m_mainMenuItems.clear();
    m_subMenus.clear();
    LoadConfig();
}

wxString Config::FindConfigFile() const {
    wxArrayString searchPaths;
    
    // For bundled app: Contents/Resources/SprintToolBox.ini
    wxString resourcePath = wxStandardPaths::Get().GetResourcesDir() + "/SprintToolBox.ini";
    searchPaths.Add(resourcePath);
    
    // For development: same directory as executable
    wxString appDir = wxStandardPaths::Get().GetExecutablePath();
    wxFileName appFileName(appDir);
    wxString appDirPath = appFileName.GetPath() + "/SprintToolBox.ini";
    searchPaths.Add(appDirPath);
    
    // Current directory
    searchPaths.Add("SprintToolBox.ini");
    
    for (const auto& path : searchPaths) {
        if (wxFileExists(path)) {
            wxLogMessage("Found config at: %s", path);
            return path;
        }
    }
    
    wxLogWarning("SprintToolBox.ini not found in any search location");
    wxLogWarning("Please create SprintToolBox.ini from SprintToolBox.ini.example");
    
    return searchPaths[0]; // Return first path for error messages
}

void Config::LoadConfig() {
    m_configPath = FindConfigFile();
    
    if (!wxFileExists(m_configPath)) {
        return;
    }
    
    wxFileInputStream input(m_configPath);
    if (!input.IsOk()) {
        wxLogError("Cannot open config file: %s", m_configPath);
        return;
    }
    
    wxTextInputStream text(input);
    wxString currentSection;
    std::vector<wxString> lines;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        if (line.StartsWith("[") && line.EndsWith("]")) {
            currentSection = line.SubString(1, line.Length() - 2);
        }
        if (currentSection == "JIRA" && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString key = line.Left(pos).Trim();
                wxString value = line.Mid(pos + 1).Trim();
                if (key.CmpNoCase("Email") == 0) m_jiraEmail = value;
                if (key.CmpNoCase("APIToken") == 0) m_jiraToken = value;
                if (key.CmpNoCase("BaseURL") == 0) m_jiraBaseURL = value;
                if (key.CmpNoCase("BoardID") == 0) {
                    long boardId;
                    if (value.ToLong(&boardId)) {
                        m_jiraBoardID = (int)boardId;
                    }
                }
            }
        }
    }
    
    // Load menu items
    LoadMainMenu();
}

void Config::LoadMainMenu() {
    if (!wxFileExists(m_configPath)) {
        return;
    }
    
    wxFileInputStream input(m_configPath);
    wxTextInputStream text(input);
    bool inMainMenu = false;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        
        if (line == "[MainMenu]") {
            inMainMenu = true;
            continue;
        }
        
        if (inMainMenu && line.StartsWith("[")) {
            break;
        }
        
        if (inMainMenu && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString name = line.Left(pos).Trim();
                wxString value = line.Mid(pos + 1).Trim();
                
                if (name == "---" || value == "separator") {
                    m_mainMenuItems.push_back(MenuItem::Separator());
                } else if (value.StartsWith("submenu:")) {
                    wxString submenuSection = value.Mid(8);
                    LoadSubmenu(submenuSection);
                    // Store submenu reference
                    MenuItem item(name, "submenu:" + submenuSection);
                    m_mainMenuItems.push_back(item);
                } else {
                    m_mainMenuItems.push_back(MenuItem(name, value));
                }
            }
        }
    }
}

void Config::LoadSubmenu(const wxString& section) {
    if (!wxFileExists(m_configPath) || m_subMenus.count(section) > 0) {
        return;
    }
    
    wxFileInputStream input(m_configPath);
    wxTextInputStream text(input);
    bool inSection = false;
    std::vector<MenuItem> items;
    
    while (!input.Eof()) {
        wxString line = text.ReadLine().Trim();
        
        if (line == "[" + section + "]") {
            inSection = true;
            continue;
        }
        
        if (inSection && line.StartsWith("[")) {
            break;
        }
        
        if (inSection && !line.IsEmpty() && !line.StartsWith("#")) {
            int pos = line.Find('=');
            if (pos != wxNOT_FOUND) {
                wxString name = line.Left(pos).Trim();
                wxString url = line.Mid(pos + 1).Trim();
                items.push_back(MenuItem(name, url));
            }
        }
    }
    
    m_subMenus[section] = items;
}
