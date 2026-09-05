# ActivityOS

<img src="resources/icons/activityos.png" alt="ActivityOS icon" width="72" height="72">

**Local-first desktop analytics for how you actually spend your workday.**

ActivityOS tracks which applications you use and turns that into explainable focus metrics — sessions, distractions, baselines, and goals. Everything stays on your machine. No accounts, no cloud, no keylogging, no screenshots.

Built with **C++20**, **Qt 6**, and **SQLite**.

![Today dashboard](docs/screenshots/today.png)

## Why it exists

Most productivity tools either invade privacy or give you opaque scores. ActivityOS is built for personal reflection:

- Records **foreground app + idle time** only
- Stores data in a local SQLite database you control
- Scores and recommendations are **deterministic and explainable**
- Optional demo data so you can explore the UI without waiting for history

## Features

- Native foreground-app and idle detection (macOS, Windows, Linux/X11)
- Session, workday, and context-switch detection
- Browser-aware classification (study / work sites vs streaming distractions)
- Editable classification rules
- Daily timeline with hover details, app breakdown, focus sessions, trends
- 14-day baselines, workstyle profile, and productivity scoring
- Goals, experiments, CSV/JSON export, pause, exclusions, and delete controls

## Status

| Platform | Status |
|----------|--------|
| **macOS (Apple Silicon)** | Primary target — install script + dashboard polish |
| Windows | Builds and tracks; packaging less polished |
| Linux/X11 | Supported |
| Linux/Wayland | Limited — OS often blocks global window inspection |

Downloaded DMGs can hit macOS Gatekeeper. Prefer building from source (below).

## Quick start (macOS)

```sh
brew install cmake qt
git clone https://github.com/eyaghmour07/ActivityOS.git
cd ActivityOS
chmod +x scripts/install_macos.sh
./scripts/install_macos.sh
```

That builds ActivityOS and installs it to `/Applications`. Full friend-share notes: [docs/share-with-a-friend.md](docs/share-with-a-friend.md).

### Update later

```sh
cd ActivityOS
git pull
./scripts/install_macos.sh
```

## Build and test

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Custom Qt path:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build
ctest --test-dir build --output-on-failure
```

### Ubuntu/Debian

```sh
sudo apt install cmake g++ qt6-base-dev libsqlite3-dev libx11-dev libxss-dev
```

### Windows

Install Qt 6, CMake, Visual Studio 2022 (Desktop C++), and SQLite. Set `CMAKE_PREFIX_PATH` if needed.

## Privacy

- **Recorded:** app names, transitions, idle duration, derived metrics, your rules/goals
- **Not recorded:** keystrokes, screenshots, webcam, file/page contents, cloud sync
- Window titles are inspected only when needed for classification and are **not stored by default**

Details: [docs/privacy.md](docs/privacy.md) · metric definitions: [docs/metrics.md](docs/metrics.md)

Data location:

- macOS: `~/Library/Application Support/ActivityOS/ActivityOS/activityos.db`
- Windows: `%LOCALAPPDATA%/ActivityOS/ActivityOS/activityos.db`
- Linux: `~/.local/share/ActivityOS/ActivityOS/activityos.db`

## Architecture

```text
Native OS adapter
  → Tracker service
  → SQLite event/session store
  → Deterministic analytics engine
  → Qt desktop dashboard
```

- [`include/activityos/activity_source.hpp`](include/activityos/activity_source.hpp) — native tracking contract
- [`include/activityos/storage.hpp`](include/activityos/storage.hpp) — SQLite + privacy ops
- [`include/activityos/analytics.hpp`](include/activityos/analytics.hpp) — workstyle metrics
- [`include/activityos/services.hpp`](include/activityos/services.hpp) — orchestration
- [`src/app`](src/app) — Qt dashboard, tray, settings, goals

## License

MIT — see [LICENSE](LICENSE).
