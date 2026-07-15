# SprintToolBox Application

This project is a system tray application built using wxWidgets, designed as a DT helper tool. It locks shortcuts and features in system tray. Remains in dock as a current sprint number. 

Download your ready binaries from Releases section (upper right).

## Configuration — SprintToolBox.ini

On first launch the application copies the bundled `SprintToolBox.ini` template to your home directory (`~/SprintToolBox.ini`) and reads it from there on every subsequent start. Open that file and fill in your own values — the app picks up changes automatically.

### [JIRA] section

Connects the app to your Jira instance so it can display the current sprint number in the dock icon.

| Key | Description |
|-----|-------------|
| `Email` | Your Atlassian account e-mail |
| `APIToken` | Personal API token — generate one at id.atlassian.com → Security → API tokens |
| `BaseURL` | Root URL of your Jira site, e.g. `https://your-org.atlassian.net` |
| `BoardID` | Numeric ID of the Scrum board whose active sprint is shown (find it in the board URL) |

### [Display] section

Toggles individual menu items on or off. Any of the standard boolean strings are accepted: `1`/`true`/`yes` to show, `0`/`false`/`no` to hide.

| Key | Controls |
|-----|----------|
| `ShowUnixTimestamp` | Unix epoch entry (click to copy) |
| `ShowZuluTimestamp` | Zulu (UTC) timestamp entry |
| `ShowTimeConverter` | Time-converter dialog |
| `ShowHexDecConverter` | Hex/Dec converter dialog |

### [MainMenu] section

Defines the top-level menu items that appear when you click the tray icon. Up to 20 entries are supported.

Each line follows one of three formats:

```ini
# Plain URL link
Label=https://example.com

# Submenu reference — items come from a separate [SectionName] section
Label=submenu:SectionName

# Visual separator
---=separator
```

### Submenu sections

Any section whose name is referenced with `submenu:SectionName` from `[MainMenu]` becomes a submenu. Each section supports up to 15 `Label=URL` entries. The bundled file ships with four example submenus:

| Section | Purpose |
|---------|---------|
| `[SyntheticControl]` | Links to Synthetic Control environments (Dev / Sprint / Prod) |
| `[CloudControl]` | Links to Cloud Control environments |
| `[MissionControl]` | Links to Mission Control environments |
| `[Monitoring]` | Links to tenant monitoring dashboards |

Add your own submenus by creating a new `[SectionName]` section and adding a `Label=submenu:SectionName` entry in `[MainMenu]`.

## Usage

The application will start in the system tray. (win-Right)Click the tray icon to access:
- Current Unix and Zulu timestamps (click to copy to clipboard)
- Hex/Dec Converter dialog
- your own shortcuts

