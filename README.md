# Sharkord Desktop

Electron desktop app for [Sharkord](https://github.com/sharkord/sharkord). Use Sharkord outside of the browser. Features communites, clientside input settings, and multi server support!

**Credits.** Sharkord is by [Diogo Martino](https://github.com/sharkord/sharkord). This desktop client is unofficial and not affiliated with the Sharkord project.

All of your Sharkord servers in one app. With multiserver support and the keep connected toggle, chatting between multiple servers is easy!
<img width="1920" height="1018" alt="{FA9B7FAB-FFFF-4CC7-B68F-0FB58534E67D}" src="https://github.com/user-attachments/assets/03bcc7c1-9059-4ad6-a114-1b89024e5114" />

Integrated communities button. View a range of servers that welcome people to join and interact!
<img width="1920" height="1016" alt="{AD0C558A-7820-4FF5-BCB5-02424C2B47F4}" src="https://github.com/user-attachments/assets/00e90dd4-4fdb-44bd-948e-8746edb65201" />

Clientside input settings gives you more control on which devices a server can see and interact with. Use push to talk to prevent annoying background noise from bothering your friends!
<img width="1920" height="1017" alt="{03B2CCFE-54F6-4B93-B4A1-514280D38B29}" src="https://github.com/user-attachments/assets/c4c26de1-ac3a-47c5-81e1-d437aa048c4a" />

## Recent Changes (2026-03-28)

### Streaming & WebRTC
- **Forced H264 encoding** via `setCodecPreferences` — uses hardware encoders automatically (NVENC on NVIDIA, AMF on AMD, QSV on Intel, OpenH264 software fallback)
- **Codec selector UI** — dropdown in top-left bar lets users choose H264 (default), VP8, VP9, or AV1. Preference is persisted
- **Forced bitrate** — sets `minBitrate = maxBitrate` on WebRTC encodings plus SDP `b=AS:` bandwidth to bypass the bandwidth estimator. Default 5 Mbps
- **Bitrate selector UI** — dropdown in top-left bar (Auto, 1-15 Mbps). Changes apply live to active streams
- **`degradationPreference: maintain-resolution`** — prevents the encoder from dropping resolution under congestion
- **Default screen share: 1080p 60fps** — `getDisplayMedia` constraints injected with `{ideal: 1920}x{ideal: 1080}@{ideal: 60}`
- **Chromium flags** — enabled `WebRtcH264WithOpenH264FFmpeg`, `enable-gpu-rasterization`, `WebRTC-Video-Pacing`
- **WebRTC stats** — now resolve actual codec `mimeType` (e.g. `video/H264`) and `encoderImplementation` (e.g. `NVIDIA H.264 Encoder MFT`)

### Screen Sharing
- **Auto-select EscapeFromTarkov** — screen picker pre-selects EFT/Arena window if running (user still clicks Share to confirm)
- **Mute incoming streams by default** — intercepts `srcObject` setter on `<video>` elements; sets `muted=true, volume=0` when a MediaStream with video tracks is assigned. Users can unmute via the web app's controls

### Push-to-Talk
- **All keyboard keys supported** — PTT key capture now accepts any `e.code` (BracketLeft, Semicolon, Slash, etc.), not just `Key`-prefixed codes
- **Background poller VK mappings** — added Windows virtual key codes for `[ ] \ ; ' , . / \` - =` so PTT works when app is unfocused
- **Friendly display names** — shows `[` instead of `BracketLeft`, `Left Shift` instead of `ShiftLeft`, etc.

### Build
- **Build ID timestamp** — format is now `{gitHash}.{MMDD}.{HHMM}` (e.g. `755f554.0328.2143`) for unique identification without new commits

### Known Issues / TODO
- `minBitrate` is not in the WebRTC spec — Chromium silently ignores it. The stable bitrate comes from the other changes (H264/NVENC, `degradationPreference`, SDP `b=AS:`), not from `minBitrate`
- Process audio resampler has a boundary interpolation bug at packet edges when upsampling — may cause minor audio glitches
- `getClipboardText` and `downloadUrl` IPC handlers are missing from preload — paste and image download in iframe context menu are non-functional
- AudioContext is leaked on each `getUserMedia` call with volume adjustment (browsers limit ~6-8)

## Privacy

Sharkord Desktop does not collect, transmit, or store any user data. All communication is between the client and the Sharkord server(s) you connect to. No analytics, telemetry, or tracking is included.

## Code Signing Policy

Releases are signed using a certificate provided by [SignPath Foundation](https://signpath.org).

**Roles:**
- **Author & Approver:** [daelsc](https://github.com/daelsc)

All signed releases are built from this public repository using GitHub Actions. Only commits to the `main` branch are eligible for signing.

Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).
