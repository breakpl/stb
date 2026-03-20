' setup_autostart_win.vbs - Creates a startup shortcut for SprintToolBox
' Usage: cscript //nologo setup_autostart_win.vbs

Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

' Get the script's directory
ScriptPath = WScript.ScriptFullName
ScriptDir = fso.GetParentFolderName(ScriptPath)

' Path to the executable
ExePath = fso.BuildPath(ScriptDir, "SprintToolBox.exe")

If Not fso.FileExists(ExePath) Then
    WScript.Echo "ERROR: SprintToolBox.exe not found in " & ScriptDir
    WScript.Quit 1
End If

' Create shortcut in Startup folder
StartupFolder = WshShell.SpecialFolders("Startup")
ShortcutPath = fso.BuildPath(StartupFolder, "SprintToolBox.lnk")

Set Shortcut = WshShell.CreateShortcut(ShortcutPath)
Shortcut.TargetPath = ExePath
Shortcut.WorkingDirectory = ScriptDir
Shortcut.Description = "Sprint tracking tray utility"
Shortcut.WindowStyle = 1  ' Normal window
Shortcut.Save

WScript.Echo "Autostart enabled: shortcut created in Startup folder"
WScript.Echo ShortcutPath
