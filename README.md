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
Version format is `0.1.{patch}` (e.g. `0.1.52`, `0.1.53`). `release.bat` auto-increments the patch number from the current `package.json` version.

### Prerequisites
- Node.js 20+
- `GH_TOKEN` environment variable set to a GitHub personal access token with `repo` scope (create at https://github.com/settings/tokens, then `setx GH_TOKEN "ghp_yourtoken"`)
- Git config set: `user.name` and `user.email` (set in repo with `git config`)

### Local Development
```
npm install                  # install dependencies (includes custom NVENC Electron)
npm run build                # compile TypeScript + minify wrapper
npm run dev                  # build and launch the app
pack.bat                     # build + package NSIS installer + portable exe into out/ (local only, no publish)
```

### Publishing a Release
Run `release.bat` from the project folder. It will:
1. Increment patch version (0.1.52 → 0.1.53)
2. Build TypeScript + minify
3. Run `electron-builder --publish always` which creates the GitHub Release and uploads the installer, blockmap, and `latest.yml` in one atomic step
4. If publish succeeds: commit version bump and push to git
5. If publish fails: revert version and exit

### CRITICAL RULES — Do Not Break These
These rules exist because we broke the release pipeline multiple times learning them:

1. **`release.bat` is the ONLY way to publish.** Never manually `gh release create`, never manually upload assets. electron-builder's `--publish always` handles everything atomically — installer, blockmap, `latest.yml`, correct filenames.

2. **Never build an installer without bumping the version.** NSIS won't overwrite an installed version with the same version number. The installer silently does nothing. Always increment the patch version before building.

3. **Git commit happens AFTER successful publish, not before.** This prevents "version bumped in git but no release on GitHub" partial failure states. `release.bat` reverts the version on failure.

4. **The `GH_TOKEN` must be a classic token with `repo` scope.** Fine-grained tokens get 403 errors on release creation. Classic tokens start with `ghp_`. Set via `setx GH_TOKEN "ghp_..."`.

5. **Every release must include the `.blockmap` file.** Without it, the updater can't do differential downloads and falls back to full 111MB downloads that may hang. electron-builder uploads this automatically — which is why rule #1 exists.

6. **The `releaseType: release` setting in package.json is required.** Without it, electron-builder creates draft releases that the updater can't see. This is configured in `package.json` under `build.publish`.

7. **Don't use `pack.bat` or `npm run pack` for releases.** Those are for local dev testing only. They don't publish, don't bump versions, and don't upload anything.

8. **The updater only works with the NSIS installer**, not the portable exe. Users running the portable version get no auto-updates.

### How Auto-Update Works
1. App launches → `NsisUpdater` checks GitHub Releases API for the latest non-draft release
2. Reads `latest.yml` from that release to get the version and download URL
3. If newer version exists, downloads the installer (differential via blockmap if available, full download otherwise)
4. User gets a dialog: "Restart Now" or "Later"
5. On restart, the NSIS installer runs silently with `--updated --force-run`

**Updater log:** `%APPDATA%\sharkov-desktop\updater.log` — check this if updates aren't working

**Cache dir:** `%LOCALAPPDATA%\sharkov-desktop-updater\pending\` — downloaded installers land here

### Scripts
- `run.bat` / `run.ps1` — build and launch the app locally (dev mode)
- `pack.bat` — build + package locally (no publish, for testing only)
- `release.bat` — **the only way to publish** (bump version, build, publish to GitHub, commit)

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
