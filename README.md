# Sharkov

Sharkov is a desktop app by [daelsc](https://github.com/daelsc), forked from [Sharkord Desktop](https://github.com/Bugel/sharkorddesktop) — an Electron client for [Sharkord](https://github.com/sharkord/sharkord), a self-hosted voice, video, and text chat platform by [Diogo Martino](https://github.com/sharkord/sharkord).

All of your Sharkord servers in one app. With multiserver support and the keep connected toggle, chatting between multiple servers is easy!
<img width="1920" height="1018" alt="{FA9B7FAB-FFFF-4CC7-B68F-0FB58534E67D}" src="https://github.com/user-attachments/assets/03bcc7c1-9059-4ad6-a114-1b89024e5114" />

Integrated communities button. View a range of servers that welcome people to join and interact!
<img width="1920" height="1016" alt="{AD0C558A-7820-4FF5-BCB5-02424C2B47F4}" src="https://github.com/user-attachments/assets/00e90dd4-4fdb-44bd-948e-8746edb65201" />

Clientside input settings gives you more control on which devices a server can see and interact with. Use push to talk to prevent annoying background noise from bothering your friends!
<img width="1920" height="1017" alt="{03B2CCFE-54F6-4B93-B4A1-514280D38B29}" src="https://github.com/user-attachments/assets/c4c26de1-ac3a-47c5-81e1-d437aa048c4a" />

## Features (added in Sharkov fork)

### Streaming & WebRTC
- **Forced H264 encoding** via `setCodecPreferences` — uses hardware encoders automatically (NVENC on NVIDIA, AMF on AMD, QSV on Intel, OpenH264 software fallback)
- **Codec selector UI** — dropdown in top-left bar lets users choose H264 (default), VP8, VP9, or AV1. Preference is persisted
- **Forced bitrate** — sets `maxBitrate` on WebRTC encodings plus SDP `b=AS:` bandwidth to bypass the bandwidth estimator. Default 5 Mbps
- **Bitrate selector UI** — dropdown in top-left bar (Auto, 1-15 Mbps). Changes apply live to active streams
- **`degradationPreference: maintain-resolution`** — prevents the encoder from dropping resolution under congestion
- **Default screen share: 1080p 60fps** — `getDisplayMedia` constraints injected with `{ideal: 1920}x{ideal: 1080}@{ideal: 60}`
- **Chromium flags** — enabled `WebRtcH264WithOpenH264FFmpeg`, `enable-gpu-rasterization`, `WebRTC-Video-Pacing`
- **WebRTC stats** — resolves actual codec `mimeType` (e.g. `video/H264`) and `encoderImplementation` (e.g. `NVIDIA H.264 Encoder MFT`)

### Screen Sharing
- **Auto-select EscapeFromTarkov** — screen picker pre-selects EFT/Arena window if running (user still clicks Share to confirm)
- **Mute incoming streams by default** — intercepts `srcObject` setter on `<video>` elements; sets `muted=true, volume=0` when a MediaStream with video tracks is assigned. Users can unmute via the web app's controls

### Push-to-Talk
- **All keyboard keys supported** — PTT key capture now accepts any `e.code` (BracketLeft, Semicolon, Slash, etc.), not just `Key`-prefixed codes
- **Background poller VK mappings** — added Windows virtual key codes for `[ ] \ ; ' , . / ` - =` so PTT works when app is unfocused
- **Friendly display names** — shows `[` instead of `BracketLeft`, `Left Shift` instead of `ShiftLeft`, etc.

### Auto-Update
- **electron-updater** checks GitHub Releases on launch, downloads in background, prompts user to restart
- Uses `NsisUpdater` directly (not the default `autoUpdater`) to avoid redirect issues with the custom NVENC-patched Electron build
- Code signature verification is disabled since builds are not code-signed
- Only works with the **NSIS installer** version, not the portable exe

### Planned: Unified Settings
Goal: Replace the two separate settings UIs (Electron gear + web app gear) with a single Electron-native settings panel.

The web app settings (`apps/client/src/components/server-screens/user-settings/`) has 5 tabs:
- **Profile** — username, bio, banner color, avatar, banner image. Uses `trpc.users.update.mutate()` — needs server API auth
- **Devices** — audio/video selection. Already replicated in Electron device settings modal
- **Password** — change password. Uses `trpc.users.updatePassword.mutate()` — needs server API auth
- **Notifications** — browser notification toggles (all messages, mentions, DMs). Client-side localStorage, easy to replicate
- **Others** — auto-join last channel, language switcher. Client-side localStorage, easy to replicate

Approach: Replicate Notifications and Others first (no API needed), then Profile and Password (requires tRPC client setup with auth). Once all tabs are replicated, hide the web app's settings gear via CSS injection.

Server source: `~/git/sharkord` (WSL) / [daelsc/sharkord](https://github.com/daelsc/sharkord)

### Known Issues / TODO
- `minBitrate` is not in the WebRTC spec — Chromium silently ignores it. The stable bitrate comes from H264/NVENC, `degradationPreference`, and SDP `b=AS:`
- Process audio resampler has a boundary interpolation bug at packet edges when upsampling — may cause minor audio glitches
- `getClipboardText` and `downloadUrl` IPC handlers are missing from preload — paste and image download in iframe context menu are non-functional
- AudioContext is leaked on each `getUserMedia` call with volume adjustment (browsers limit ~6-8)

## Building & Releasing

### Versioning
Version format is `0.1.{build}` where `{build}` auto-increments from `.buildcount` file (local, not committed). The `.buildcount` file persists between builds. `release.bat` handles incrementing automatically.

### Prerequisites
- Node.js 20+
- `gh` CLI authenticated in WSL (`wsl -- gh auth status` to verify)
- Git config set: `user.name` and `user.email` (set in repo with `git config`)

### Local Development
```
npm install                  # install dependencies (includes custom NVENC Electron)
npm run build                # compile TypeScript + minify wrapper
npm run dev                  # build and launch the app
npm run pack                 # build + package NSIS installer + portable exe into out/
```

### Publishing a Release
Run `release.bat` from the project folder. It will:
1. Auto-increment version from `.buildcount` (e.g. `0.1.52`, `0.1.53`, ...)
2. Build the NSIS installer and generate `latest.yml` + blockmap
3. Copy installer with hyphenated filename (GitHub mangles spaces in uploads)
4. Prompt to confirm the release
5. Commit version bump and push to GitHub
6. Create a GitHub Release on `daelsc/sharkov-desktop`
7. Upload installer, blockmap (for differential updates), and `latest.yml`

### How Auto-Update Works
1. App launches → `NsisUpdater` checks `latest.yml` from the latest GitHub Release
2. If a newer version exists, it downloads the installer in the background
3. User gets a dialog: "Restart Now" or "Later"
4. On restart (or next quit), the NSIS installer runs silently and relaunches the app

**Important files:**
- `latest.yml` — tells the updater what version is available and the download URL
- `.blockmap` — enables differential updates (only download changed bytes). Without it, falls back to full download (~111MB)
- Asset filenames must use **hyphens** not spaces (e.g. `Sharkov-Setup-0.1.51.exe`) — `release.bat` handles this automatically

**Updater log:** `%APPDATA%\sharkov-desktop\updater.log` — check this if updates aren't working

### Other Scripts
- `run.bat` / `run.ps1` — build and launch the app locally (dev mode)
- `pack.bat` — build + package both NSIS installer and portable exe into `out/`
- `release.bat` — full release workflow (build, package, publish to GitHub)

### Architecture Notes
- The app wraps Sharkord web app in an Electron iframe and injects JavaScript to control WebRTC, device selection, PTT, and screen sharing
- Internal message types (`sharkord-ptt`, `sharkord-set-video-bitrate`, etc.) use the original `sharkord` prefix for compatibility with the server iframe protocol
- User-visible strings use `Sharkov`
- Uses a [custom NVENC-patched Electron](https://github.com/steveseguin/electroncapture) build for hardware H264 encoding in WebRTC
- Native C++ addon (`native/`) provides per-process audio capture via Windows `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK` API
- There are **two separate settings UIs**: the Electron client's device settings modal (audio/video/PTT) and the Sharkord web app's settings (profile, etc.) inside the iframe. These are independent — merging them would require server-side changes
- Local project folder is `C:\Users\dave\git\sharkorddesktop` (not renamed from the original)
- Git config for this repo: `user.name = "dave"`, `user.email = "daelsc@users.noreply.github.com"` (set locally, not global)

## Privacy

Sharkov does not collect, transmit, or store any user data. All communication is between the client and the Sharkord server(s) you connect to. No analytics, telemetry, or tracking is included.
