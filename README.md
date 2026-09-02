# ActivityOS

ActivityOS is a local-first desktop application that measures application usage and turns it into explainable workstyle analytics. It tracks active applications and idle time—not keystrokes, screenshots, webcam data, or document contents.

The app is built with C++20, Qt 6, and SQLite. It runs from the system tray and opens as a normal desktop dashboard.

## Features

- Native foreground-application and idle detection for macOS, Windows, and Linux/X11
- Session, workday, and context-switch detection
- User-editable application and optional window-title classification rules
- Browser-aware classification that separates common study/development sites from streaming distractions
- Local SQLite history with safe migrations, transactions, and retention cleanup
- Daily and weekly activity summaries
- Focus and deep-work detection
- Distraction severity, recovery time, and estimated distraction cost
- Session distributions, common transitions, peak hours, and work-type distribution
- Personal 14-day baselines, comparisons, trends, and workstyle profile
- Explainable, configurable productivity scoring
- Goals, productivity experiments, and deterministic recommendations
- CSV/JSON export, pause, exclusions, date-range deletion, and delete-all
- Safe synthetic demo data for exploring every dashboard

The optional AI narration layer from the PRD is intentionally not included.

## Requirements

- CMake 3.24+
- A C++20 compiler
- Qt 6.5+ with Core, Gui, and Widgets
- SQLite 3 development files
- Linux only: X11 and XScreenSaver development files

### macOS

```sh
brew install cmake qt
```

### Ubuntu/Debian

```sh
sudo apt install cmake g++ qt6-base-dev libsqlite3-dev libx11-dev libxss-dev
```

### Windows

Install Qt 6, CMake, Visual Studio 2022 with the Desktop C++ workload, and SQLite development files. Set `CMAKE_PREFIX_PATH` to the Qt installation if CMake cannot locate it.

## Build and test

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

If Qt is installed in a custom location:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

macOS:

```sh
open build/ActivityOS.app
```

Windows:

```powershell
.\build\Debug\ActivityOS.exe
```

Linux:

```sh
./build/ActivityOS
```

On first launch, ActivityOS explains exactly what it records and requests consent. Closing the dashboard keeps the tray application running; use the tray menu to pause or quit. Launch-at-login is optional.

Data is stored in the operating system’s standard local application-data directory:

- macOS: `~/Library/Application Support/ActivityOS/ActivityOS/activityos.db`
- Windows: `%LOCALAPPDATA%/ActivityOS/ActivityOS/activityos.db`
- Linux: `~/.local/share/ActivityOS/ActivityOS/activityos.db`

## Platform notes

- **macOS:** Foreground application and idle duration use AppKit and CoreGraphics. Chrome tab sites are read via Apple Automation when allowed; other browsers fall back to window titles. URLs and titles are not persisted by default.
- **Windows:** Foreground application and idle duration use Win32 APIs.
- **Linux/X11:** Active-window metadata uses EWMH/X11 and idle duration uses XScreenSaver.
- **Linux/Wayland:** Global active-window inspection is intentionally restricted by many compositors. ActivityOS reports this limitation instead of recording misleading data; an X11/XWayland session is currently required.

## Architecture

```text
Native OS adapter
  → Tracker service
  → SQLite event/session store
  → Deterministic analytics engine
  → Qt desktop dashboard
```

The layers are independent:

- [`include/activityos/activity_source.hpp`](include/activityos/activity_source.hpp): native tracking contract and test fake
- [`include/activityos/storage.hpp`](include/activityos/storage.hpp): SQLite repositories and privacy operations
- [`include/activityos/analytics.hpp`](include/activityos/analytics.hpp): deterministic workstyle metrics
- [`include/activityos/services.hpp`](include/activityos/services.hpp): tracker and dashboard orchestration
- [`src/app`](src/app): Qt dashboard, tray, onboarding, settings, goals, and experiments

See [`docs/metrics.md`](docs/metrics.md) for definitions and formulas and [`docs/privacy.md`](docs/privacy.md) for the data policy.

## Package

```sh
cmake --build build --target package
```

This creates a DMG on macOS, an NSIS installer on Windows when NSIS is installed, and a compressed package on Linux.
