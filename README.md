# wxWidgets System Tray Application

This project is a system tray application built using wxWidgets, designed as a learning project and alternative to Qt. It demonstrates system tray icons, popup menus, clipboard operations, and dialog windows.

## Features

- **System Tray Icon**: Runs in the system tray with a custom icon
- **Timestamp Utilities**: 
  - Copy current Unix timestamp to clipboard
  - Copy current Zulu (ISO 8601) timestamp to clipboard
- **Hex/Decimal Converter**: Dialog window for converting between hexadecimal and decimal numbers
- **Cross-platform**: Works on macOS, Windows, and Linux

## Project Structure

```
wxwidgets-learning-project
├── src
│   ├── main.cpp              # Entry point - creates tray icon app
│   ├── TrayIconApp.cpp       # System tray icon implementation
│   └── ConverterDialog.cpp   # Hex/Dec converter dialog
├── include
│   ├── TrayIconApp.h         # Tray icon class declaration
│   └── ConverterDialog.h     # Converter dialog declaration
├── CMakeLists.txt            # CMake configuration file
└── README.md                 # Project documentation
```

## Setup Instructions

1. **Install wxWidgets**: Make sure you have wxWidgets installed on your system.
   
   On macOS with Homebrew:
   ```bash
   brew install wxwidgets
   ```
   
   On Ubuntu/Debian:
   ```bash
   sudo apt-get install libwxgtk3.0-gtk3-dev
   ```

2. **Clone the repository**: 
   ```bash
   git clone <repository-url>
   cd wxwidgets-learning-project
   ```

3. **Build the project**:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

## Usage

After building the project, run the application:
```bash
./wxwidgets-learning-project
```

The application will start in the system tray. Right-click the tray icon to access:
- Current Unix and Zulu timestamps (click to copy to clipboard)
- Hex/Dec Converter dialog
- Quit option

## Learning Points

This project demonstrates:
- **wxTaskBarIcon**: Creating system tray applications
- **wxMenu**: Dynamic menu creation and event handling
- **wxDialog**: Creating modal and non-modal dialog windows
- **wxClipboard**: Clipboard operations
- **Event handling**: Binding events to methods
- **Cross-platform UI**: Using wxWidgets for portable applications

## Additional Information

This project serves as a foundation for learning wxWidgets. You can expand upon it by adding more features, experimenting with different UI components, and exploring event handling in wxWidgets.