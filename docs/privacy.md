# ActivityOS privacy model

ActivityOS is a single-user, local-only application. It does not require an account, server, cloud service, or internet connection.

## Recorded by default

- Foreground application name
- Application transition timestamps
- OS-reported idle duration
- Derived sessions, context switches, and workstyle metrics
- User-created rules, goals, and experiments

Window titles may be inspected transiently to apply an enabled classification rule, but they are not persisted unless the user explicitly enables title storage.

## Never recorded

- Keystrokes or typed text
- Screenshots or screen video
- Webcam or microphone
- File contents, messages, source code, or web-page contents
- Emotion or identity data
- Employer/team monitoring data

## User controls

- Pause or resume tracking
- Exclude applications and categories before activity is stored
- Disable window-title persistence
- Export activity as CSV or JSON
- Delete a selected date range
- Delete all recorded activity
- Disable launch at login

The SQLite database and logs use the operating system’s application-data directory and normal per-user filesystem protections. ActivityOS currently does not add independent database encryption; users requiring encrypted storage should enable full-disk encryption such as FileVault, BitLocker, or LUKS.

## Platform limitations

Operating systems may withhold window metadata unless the user grants permission. ActivityOS treats missing data as a degraded capability and does not attempt to bypass OS privacy controls. Linux Wayland often prohibits global foreground-window discovery; ActivityOS reports that collection is unsupported or incomplete instead of silently producing inaccurate analytics.

## Interpretation

ActivityOS is designed for personal reflection, not employee surveillance. Scores and recommendations are based on configurable heuristics and personal historical baselines. They must not be treated as objective measures of job performance, effort, or wellbeing.
