# ActivityOS

[![build-and-test](https://github.com/eyaghmour07/ActivityOS/actions/workflows/ci.yml/badge.svg)](https://github.com/eyaghmour07/ActivityOS/actions/workflows/ci.yml)

<img src="resources/icons/activityos.png" alt="ActivityOS icon" width="72" height="72">

**Local-first desktop analytics for how you actually spend your workday.**

ActivityOS tracks which applications you use and turns that into explainable focus metrics — sessions, distractions, baselines, and goals. Everything stays on your machine. No accounts, no cloud, no keylogging, no screenshots.

Built with **C++20**, **Qt 6**, and **SQLite**.

![Today dashboard](docs/screenshots/today.png)

![Walkthrough of Today, Focus, Apps, Trends, Goals, and Settings](docs/screenshots/tour.gif)

| [Focus](docs/screenshots/focus.png) | [Trends](docs/screenshots/trends.png) |
| --- | --- |
| [Goals](docs/screenshots/goals.png) | [Settings](docs/screenshots/settings.png) |

## Why it exists

Most productivity tools either invade privacy or give you an opaque score. ActivityOS is for personal reflection: it records **foreground app + idle time** only, stores a SQLite file you control, and computes scores you can audit.

## Features

- Native foreground-app and idle detection on macOS, Windows, and Linux/X11
- Session, focus, and context-switch detection with a hoverable day timeline
- Browser-aware classification (work/study sites vs streaming) plus editable rules
- 14-day baselines, weekly trends, and an explainable productivity score
- Goals, experiments, pause/export/delete — data never leaves the device

## Design decisions

I chose **C++20 and Qt 6** because this is a tray-resident tracker that should stay cheap *while you are not looking at it*. Qt is one process and one UI toolkit — not a Chromium renderer, GPU process, and helper sitting in the dock. That is the argument, and it is about architecture and idle CPU, not about winning a RAM contest against Electron when the dashboard is on screen. Bundled Qt Widgets still cost on the order of a hundred megabytes; see Performance.

Scoring is **deterministic on purpose**. There is no model, no embeddings, no “AI insight” layer. Focus blocks, distraction cost, and the 0–100 score are formulas you can read in [`docs/metrics.md`](docs/metrics.md) and step through in tests. That is slower to look magical and faster to trust: if the number is wrong, you can find the branch. An ML classifier would hide the same mistakes behind a confidence score I could not explain in a code review.

**Why not ActivityWatch?** It is the obvious prior art: open-source, local-first, cross-platform time tracking. I did not start from it because I wanted a native C++/Qt process and a scored, explainable workstyle layer (focus blocks, baselines, goals) rather than a Python/web stack and raw time buckets. Building it was the point of the project; using ActivityWatch would have been the right choice if the goal was only “log which app is focused.”

The tradeoff I actually hit: **shipping a Qt app on macOS is harder than writing the analytics.** A CMake binary links Homebrew Qt; a `.app` you drag to `/Applications` has to bundle those frameworks or it dies at launch (“quit unexpectedly”) when two Qt copies load. `macdeployqt` plus ad-hoc codesign is the path that works. A downloaded DMG still gets Gatekeeper-quarantined, so [v0.1.0](https://github.com/eyaghmour07/ActivityOS/releases/tag/v0.1.0) is a source tag plus an install script, not a fake one-click installer.

## Performance

Measured on an Apple Silicon Mac (`2026-09-05`), same process, tracker polling every 5 s.

| Metric | Measured |
| --- | --- |
| Sampling interval | **5 s** foreground poll · **60 s** heartbeat · **30 s** UI refresh |
| Idle threshold | **5 min** of OS-reported idle before a session closes |
| RSS (dashboard open) | **167 MiB** (12 samples / 60 s, range 166.5–166.8) |
| RSS (tray only, window closed) | **167 MiB immediately**, then **~84–110 MiB** as the hidden UI is paged (60 samples / 5 min so far) |
| CPU (dashboard open, 60 s) | **0.0%** average and max (`ps` 5 s interval) |
| CPU (tray only, 5 min) | **~0%** most samples · **1.0%** on the first sample after close |
| Database (live, 3 days) | **276 KiB** main file · 736 sessions · 975 events — a week of growth is **not measured yet**; do not treat 3 days as a weekly rate |

The tray number is the one that matches the C++/Qt claim. Closing the window does not unload Qt; RSS stays at dashboard size until the OS reclaims the hidden widgets, then it drops. That is still one process and no Chromium — but it is not a 30 MiB daemon, and I would not pretend otherwise in an interview.

Demo data in this copy adds sessions (840 / 13-day span) and a WAL file until SQLite checkpoints; the 276 KiB figure is the live window before that load.

## Status

| Platform | Status |
|----------|--------|
| **macOS (Apple Silicon)** | Primary target — build via the install script |
| Windows | Builds and tracks; packaging less polished |
| Linux/X11 | Supported |
| Linux/Wayland | Limited — compositors often block global window inspection |

**[v0.1.0](https://github.com/eyaghmour07/ActivityOS/releases/tag/v0.1.0)** is a tagged source release (zip/tarball on GitHub). Build it on the machine that will run it; that avoids Gatekeeper quarantine.

## Quick start (macOS)

```sh
brew install cmake qt
git clone https://github.com/eyaghmour07/ActivityOS.git
cd ActivityOS
chmod +x scripts/install_macos.sh
./scripts/install_macos.sh
```

See [docs/share-with-a-friend.md](docs/share-with-a-friend.md) to update or share a copy.

## Build and test

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

CI runs the same four suites (**analytics**, **storage**, **activity source**, **services**) on macOS, Ubuntu, and Windows. There is no line-coverage gate yet; the suites cover classification (including VS Code / Chrome / lock-screen), sessionization, migrations, and dashboard aggregation.

### Ubuntu/Debian

```sh
sudo apt install cmake g++ qt6-base-dev libsqlite3-dev libx11-dev libxss-dev
```

### Windows

Install Qt 6, CMake, Visual Studio 2022 (Desktop C++), and SQLite. Set `CMAKE_PREFIX_PATH` if needed.

## Privacy

- **Recorded:** app names, transitions, idle duration, derived metrics, your rules/goals
- **Not recorded:** keystrokes, screenshots, webcam, file/page contents, cloud sync
- Window titles are inspected only for classification and are **not stored by default**
- Lock-screen / `loginwindow` is treated as idle, not an app

Details: [docs/privacy.md](docs/privacy.md) · formulas: [docs/metrics.md](docs/metrics.md)

Data lives at:

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
- [`src/app`](src/app) — dashboard, tray, settings, goals

## What I'd do next

- **Signed, notarized macOS install** so friends can skip building Qt. Gatekeeper is the real distribution blocker, not missing features.
- **Wayland** needs a compositor-specific path (or an honest “unsupported” forever). Silent fake data would be worse.
- **Line-coverage + fuzz on the sessionizer** so merge/idle edge cases stay pinned.
- **Windows installer polish** to match the macOS script.

Known limitations today: Linux/Wayland collection is incomplete; DMG/ZIP downloads are not a supported install path; scores are heuristics, not a measure of job performance.

## License

MIT — see [LICENSE](LICENSE).
