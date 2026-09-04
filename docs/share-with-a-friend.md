# Share ActivityOS with a friend (macOS)

Downloaded DMG/ZIP files are often blocked by macOS Gatekeeper ("app is damaged" or won't open). The reliable way is to **build from source on their Mac** — locally built apps are not quarantined.

## What your friend needs

- Mac with Apple Silicon (M1/M2/M3/M4)
- Internet connection
- About 10–15 minutes the first time (downloads Qt via Homebrew)

## Steps for your friend

1. Install Homebrew if they don't have it — https://brew.sh

2. Clone and install:

```bash
git clone https://github.com/eyaghmour07/ActivityOS.git
cd ActivityOS
chmod +x scripts/install_macos.sh
./scripts/install_macos.sh
```

3. On first launch, accept privacy consent and allow **Chrome control** + **Screen Recording** when prompted.

4. If Chrome tabs still show as General:

   - **System Settings → Privacy & Security → Automation**
   - Enable **ActivityOS** for **Google Chrome**
   - Quit and reopen ActivityOS

## Private repo?

Add your friend as a collaborator on GitHub, then they clone with their own GitHub account.

## Updating later (replace old version)

The install script automatically removes the old app from `/Applications` before installing the new one. Your tracking data is **not** deleted — it lives in `~/Library/Application Support/ActivityOS/`, separate from the app.

```bash
cd ActivityOS          # wherever you cloned it
git pull
./scripts/install_macos.sh
```

Optional cleanup (safe to delete):

```bash
# Old DMG from a previous download attempt
rm -f ~/Downloads/ActivityOS*.dmg

# Stray copy if you dragged the app to Desktop or Downloads
rm -rf ~/Desktop/ActivityOS.app ~/Downloads/ActivityOS.app
```

After updating, quit and reopen ActivityOS. macOS may ask you to re-allow Chrome Automation or Screen Recording — that's normal.
