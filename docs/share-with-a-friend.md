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

## Updating later

```bash
cd ActivityOS
git pull
./scripts/install_macos.sh
```
