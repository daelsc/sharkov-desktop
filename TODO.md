# TODO — Resume Here (Login Persistence Feature)

> **Read this first.** This file is a handoff/resume doc for the login-persistence
> work. It is intentionally tracked in git so it travels to every clone
> (including the Windows box `snowwhite`). Delete or replace this file once the
> work is shipped and verified.

**Last updated:** 2026-06-23 (on `crow`, the Linux server)
**Branch:** `main` — all work committed and pushed to `origin/main` (HEAD `caf5a72`)
**Status:** CODE COMPLETE & PUSHED. Not yet released. Manual E2E not yet run.

---

## TL;DR — what to do next

1. **Pick a release path** (see "Release decision" below) — 3 options, needs the
   user's choice. Default-safe: run `release.bat` on Windows.
2. **After release + client update, run the manual E2E** (Task 7 below) against a
   live Sharkord server. This is the ONLY untested part — it needs Windows +
   `safeStorage` (DPAPI) + a live `/login` + the real connect screen.

---

## What was built

The bug: users who check **"Login automatically"** get bounced to an empty
connect screen after their session token expires, forcing a manual re-login.
Three compounding causes; the fixable layer was in this repo.

Root-cause summary (full analysis in commit history + below):
- **sharkov-desktop:** the `desktop-get/set-credentials` IPC + preload API +
  `SavedServer.identity/password` fields existed but were **dead code** — never
  called. Now wired up end-to-end.
- **sharkord web app** (`~/git/sharkord`): reads `sharkord-identity` /
  `sharkord-user-password` from localStorage but nothing writes them;
  `auto-login-controller.tsx` self-disables (clears token + unchecks the box)
  on token expiry. The desktop fix sidesteps all of this by re-running the
  app's own `/login` to refresh the token.

**Decision made (auto-submit):** on session expiry with saved creds, the app
auto-fills the connect form, re-ticks "Login automatically", and clicks
Connect — seamless, no user clicks. (Alternative "pre-fill only" was rejected.)

---

## The 8 commits on `main` (newest last)

```
caf5a72 chore: gitignore local docs/ plan files
71ccc89 fix: close unclosed if-block in credential-capture injection IIFE
ac05035 docs: document persistent login and credential encryption
a17ab54 feat: route credential postMessages to IPC with origin validation
d6f1f98 feat: inject connect-screen auto-login replay
81ffe66 feat: inject fetch wrapper to capture credentials on login
8814895 feat: encrypt saved credentials with safeStorage, add clear-credentials IPC
fda1bd4 feat: add credentials storage module with vitest harness
```

Parent (pre-feature) release tag: `v0.1.58` (`f45465a`).

---

## Architecture of the fix (what each file does)

- **`src/credentials.ts`** (NEW, pure, unit-tested) — `saveCredentials` /
  `loadCredentials` / `clearCredentials` / `findServerByOrigin`. Generic over
  element type so it preserves `SavedServer`. Passwords are encrypted by an
  injected `CredentialCrypto` (never stored plaintext). No-op (returns input) if
  no server matches the origin — never auto-creates a server entry.
- **`src/main.ts`** —
  - `credentialCrypto` adapter wrapping Electron `safeStorage` (DPAPI on
    Windows). `null` if OS encryption unavailable → handlers refuse to store.
  - Rewrote `desktop-get/set-credentials` IPC to use the module + crypto.
  - NEW `desktop-clear-credentials` IPC.
  - `desktop-remove-server` now clears creds for that origin too.
  - NEW injection generator `getCredentialCaptureInjectionCode()` — wraps
    `window.fetch`; on `POST {origin}/login` success with body
    `{identity,password,autoLogin}`: posts `sharkord-save-credentials` (if
    autoLogin) or `sharkord-clear-credentials` (if not) to parent.
  - NEW injection generator `getAutoLoginInjectionCode()` — polls (500ms +
    MutationObserver) for `[data-testid="connect-identity-input"]`; on detection
    posts `sharkord-request-credentials`; on `sharkord-credentials` reply, sets
    identity+password via native value setter + `input`/`change` events
    (React-safe), ensures the auto-login switch is checked (Radix
    `data-state="checked"`), clicks `[data-testid="connect-button"]`. One
    attempt per page-load — no retry loop on failure.
  - Both new generators are called in `injectDevicePrefsIntoFrame()` alongside
    the existing ones.
- **`src/preload.ts`** — added `clearCredentialsForOrigin`.
- **`static/wrapper.js`** — three new `message` cases
  (`sharkord-save-credentials`, `sharkord-clear-credentials`,
  `sharkord-request-credentials`) with **origin validation**: only acts for
  origins of known saved servers; reply uses `e.origin` as `postMessage`
  targetOrigin so creds can't leak to a spoofed source. `getOrigin()` helper
  already existed in the file.
- **`test/credentials.test.ts`** + **`vitest.config.ts`** — 10 unit tests, all
  passing. Tests live in `test/` (outside `src/`) so `tsc` (rootDir `src`) never
  compiles them into `dist/`.
- **`package.json`** — added `vitest` devDep + `test` / `test:watch` scripts.
- **`README.md`** — "Persistent Login" feature section + Privacy note.

Message protocol (all use the `sharkord-` prefix for server iframe compat):
```
iframe → parent:  sharkord-save-credentials   {identity, password}
iframe → parent:  sharkord-clear-credentials
iframe → parent:  sharkord-request-credentials
parent → iframe:  sharkord-credentials          {identity, password}  (or nulls)
```

---

## Verification done so far (all green)

- `npm test` — 10/10 unit tests pass.
- `npm run check-types` — clean.
- `npm run lint` — clean.
- `npm run build` — succeeds (tsc + terser).
- **All 6 injection generators syntax-validated** by extracting the generated
  JS string and `new Function()`-ing it. (This caught a real brace bug in
  `getCredentialCaptureInjectionCode` — fixed in `71ccc89`. `tsc`/`eslint` can't
  see inside string literals, which is why this extra check matters. If you edit
  any injection generator, re-run this check — see snippet below.)

```bash
# Injection syntax-checker (run from repo root after editing any get*InjectionCode)
cat > /tmp/check-injections.mjs <<'SCRIPT'
import fs from 'node:fs'; import vm from 'node:vm';
const src = fs.readFileSync('src/main.ts','utf8');
const ctx = { prefsJson:'"{}"', pttBinding:'null', FORCED_BPS:5000000, FORCED_CODEC:'"H264"' };
const names = ['getDevicePrefsInjectionCode','getClipboardCopyInjectionCode','getMuteStreamsInjectionCode','getWebrtcStatsInjectionCode','getCredentialCaptureInjectionCode','getAutoLoginInjectionCode'];
function extract(n){const s=src.indexOf('function '+n+'(): string {');const a=src.indexOf('return [',s)+'return ['.length;const e=src.indexOf("].join('');",a);return vm.runInNewContext('(['+src.slice(a,e)+'])',ctx).join('');}
let ok=true; for(const n of names){const c=extract(n);try{new Function(c);console.log(n+': OK ('+c.length+')');}catch(e){ok=false;console.log(n+': SYNTAX ERROR -> '+e.message);}}
console.log(ok?'ALL VALID':'FAILURES');
SCRIPT
node /tmp/check-injections.mjs
```

---

## What is NOT done — the resume point

### Release decision (needs user input)

The feature is on `origin/main` but **no release has been published**, so the
auto-updater won't see it. Three options were presented to the user (not yet
chosen):

1. **Run `release.bat` on Windows** (sanctioned path, README rule #1). Bumps
   `0.1.58 → 0.1.59`, builds NSIS + blockmap + `latest.yml`, publishes, pushes
   the version-bump commit. ~5 min. This is the default-safe choice.
2. **Modify `build.yml` to publish on push** — contradicts README CRITICAL RULE
   #1 ("release.bat is the ONLY way to publish"). Re-opens past failure modes.
3. **Hybrid** — add a `workflow_dispatch` publish step to CI so a push builds
   + tests, and a manual "Run workflow" click on the GitHub Actions tab
   publishes. Git-push-to-release without the every-push footgun.

⚠️ **CI already builds an NSIS installer** (`.github/workflows/build.yml` line
~94, `--win nsis --x64 --prepackaged`), but it (a) stamps a dirty version
`0.1.58-<run>.<sha>` the updater won't recognize, and (b) only uploads as a
workflow artifact, not a GitHub Release with `latest.yml`. So CI as-is cannot
serve the updater. Options 2/3 would change this.

To release via option 1 on Windows:
```
cd C:\Users\dave\git\sharkorddesktop   # (local repo path per README)
git pull
echo %GH_TOKEN%                         # must be classic ghp_ token, repo scope
release.bat
```
Then force the client to update: quit + relaunch the **NSIS-installed** Sharkov
(not portable). Watch `%APPDATA%\sharkov-desktop\updater.log`.

### Task 7 — Manual end-to-end verification (NOT run; needs Windows + live server)

Full checklist (from the plan; the plan file itself is gitignored under
`docs/superpowers/plans/2026-06-23-persistent-login.md` on `crow` — it will NOT
be on `snowwhite`; this file is the canonical copy):

1. `npm run build` succeeds; `dist/main.js` + `dist/preload.js` written.
2. `npm run dev`; add/use a server; log in with identity+password, **check
   "Login automatically"**, click Connect. Expect: login succeeds.
3. Inspect `%APPDATA%\sharkov-desktop\config.json`. Expect: the matching server
   entry has `identity` (plaintext) and `password` (base64 ciphertext — **not**
   the real password). If `password` is the plaintext string, `safeStorage`
   encryption is broken.
4. **Simulate token expiry** (in the server iframe DevTools):
   ```js
   localStorage.removeItem('sharkord-auto-login-token');
   localStorage.setItem('sharkord-auto-login', 'false');
   location.reload();
   ```
   Expect: connect screen flashes briefly, then auto-fills and reconnects with
   no clicks. After reconnect, `localStorage['sharkord-auto-login']` is `true`
   again and `sharkord-auto-login-token` is set.
5. **Wrong-password failure (no loop):** edit the stored `password` in
   `config.json` to garbage, restart, trigger the connect screen. Expect: one
   auto-login attempt fails; connect screen stays showing the app's login error;
   no retry loop. (Fix the password back afterward.)
6. **Uncheck auto-login clears creds:** on connect screen, uncheck "Login
   automatically", click Connect. Expect: `config.json` no longer has
   `identity`/`password` for that server.
7. **Remove server clears creds:** remove the server from the sidebar. Expect:
   its `identity`/`password` gone from `config.json`.
8. Final: `npm test && npm run check-types && npm run lint` all green.

---

## Key facts the next agent needs

- **Version:** still `0.1.58` in `package.json` (release.bat bumps it).
- **Build is Windows-only** for release (NVENC-patched Electron swap + native
  WASAPI addon via MSVC). CI can build but not publish (see above).
- **`wrapper.html` loads `wrapper.js`, NOT `wrapper.min.js`** — edit
  `wrapper.js`; `npm run build` regenerates the min copy via terser for parity.
- **Sharkord connect-screen test-ids** (from `~/git/sharkord` /
  `packages/shared/src/test-ids.ts`): `connect-identity-input`,
  `connect-password-input`, `connect-button`, `connect-auto-login-switch`. The
  switch is a Radix `Switch` exposing `data-state="checked"`.
- **Sharkord login flow** (`apps/client/src/screens/connect/index.tsx`):
  `POST {url}/login` with body `{identity, password, invite, autoLogin}`;
  success returns `{token}`. Token stored in `sharkord-auto-login-token`
  localStorage; `sharkord-auto-login` bool controls auto-login.
- **Two settings UIs exist** (Electron device modal + web app gear) — merging
  them is a separate planned feature, out of scope here.
- **Known unrelated issues** (don't fix unless asked): `wrapper.min.js` is
  unreferenced; `getClipboardText`/`downloadUrl` IPC missing from preload;
  AudioContext leak on `getUserMedia` with volume adjustment.

## Process note

This work was planned with the `superpowers:writing-plans` skill and executed
with `superpowers:executing-plans` (installed as a pi package:
`git:github.com/obra/superpowers`). The full plan document lived at
`docs/superpowers/plans/2026-06-23-persistent-login.md` on `crow` but that path
is gitignored, so it is not present on `snowwhite`. This `TODO.md` is the
authoritative resume source.
