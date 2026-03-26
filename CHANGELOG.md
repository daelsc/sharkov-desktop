# Changelog

## v0.0.6-40 (2026-03-26)

Release: https://github.com/daelsc/sharkorddesktop/releases/tag/v0.0.6-40.4a35852

### Features
- **Hardware video encoding (NVENC)** — Uses Steve Seguin's patched Electron
  (`electron-v39.2.16-qp20`) to enable NVENC/AMF/QSV hardware encoding for
  WebRTC screen sharing. Custom Electron is downloaded at install time and
  swapped in after packaging.
- **Per-process audio capture** — When screen sharing on Windows, audio from a
  specific process can be captured and mixed into the stream. Uses a native
  addon (`native/`) built with `node-gyp` that calls Windows Audio Session API
  (WASAPI) via `audioclient.h` / `audiopolicy.h`. An AudioWorklet in the
  injected iframe code feeds captured PCM into a `MediaStreamDestination`.
- **Version label in sidebar** — Build version (e.g. `v0.0.6-40.4a35852`) is
  displayed at the bottom of the sidebar so users can identify their build.

### Build & CI
- Build workflow produces portable exe only (no installer).
- Version is stamped at CI time as `{version}-{run_number}.{short_sha}`
  (e.g. `0.0.6-40.4a35852`).
- Artifact named `sharkorddesktop-{stamped_version}`.
- GitHub Actions bumped to Node.js 24-compatible versions (checkout@v5,
  setup-node@v5, setup-python@v6, upload-artifact@v5).
- Removed tag trigger and auto-release job to prevent duplicate builds.
  Releases are created manually.

## Unreleased (feature/push-to-talk-improvements)

### Push-to-talk improvements
Branched from main. The upstream Sharkord codebase already includes basic PTT
(key binding UI, background polling via GetAsyncKeyState, iframe injection).
This branch fixes several gaps:

- **Accept all keyboard keys** — Binding listener no longer filters to `Key*`
  codes only. Space, Shift, F-keys, arrows, modifiers, Numpad, etc. all work.
- **Mouse-button PTT in wrapper** — Mouse bindings (middle, back, forward) now
  work when focus is on the wrapper document, not only inside iframes.
- **Visual PTT indicator** — Small mic icon in sidebar footer: red when muted
  (key not held), green when hot (key held). Hidden when no binding is set.
- **Background poll notifies renderer** — `setPttPressed` sends
  `ptt-state-change` IPC to the renderer so the indicator updates even when
  the app is unfocused (Windows background polling via GetAsyncKeyState).
- **Better display names** — Settings modal shows "Left Shift", "Space",
  "Mouse back" etc. instead of raw key codes.
- **pttBindingToVk supports raw e.code** — Background poller handles `Space`,
  `ShiftLeft`, `Digit0`, `F1`–`F24`, `NumLock`, `ScrollLock` in addition to
  the existing `Key`-prefixed format.
- **Iframe injection fix** — Changed keyboard listener condition from
  `indexOf("Key")===0` to `indexOf("Mouse")!==0` so non-letter keys work
  inside iframes too.
- **Dead code cleanup** — Removed no-op `registerPttGlobalShortcut`,
  `unregisterPttGlobalShortcut`, and unused `globalShortcut` import.
