# SprintToolBox Application

This project is a system tray application built using wxWidgets, designed as a DT helper tool. It locks shortcuts and features in system tray. Remains in dock as a current sprint number. 

## Features

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
./SprintToolBox
```

The application will start in the system tray. Right-click the tray icon to access:
- Current Unix and Zulu timestamps (click to copy to clipboard)
- Hex/Dec Converter dialog
- Quit option

