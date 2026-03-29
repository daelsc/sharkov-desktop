/**
 * Windows-only: PTT when app is in background by polling GetAsyncKeyState.
 * No global hooks—just periodic state read. Less likely to trigger keylogger detection
 * than SetWindowsHookEx-based packages.
 */

const POLL_MS = 50;

/** Map pttBinding string (e.g. "KeyP", "Mouse4") to Windows virtual key code. Returns null if unsupported. */
export function pttBindingToVk(binding: string): number | null {
  if (!binding || typeof binding !== 'string') return null;
  const s = binding.trim();
  if (s.startsWith('Mouse')) {
    const n = parseInt(s.slice(5), 10);
    // Windows VK: LBUTTON=0x01, RBUTTON=0x02, MBUTTON=0x04, XBUTTON1=0x05, XBUTTON2=0x06
    // DOM button: 0=left, 1=middle, 2=right, 3=back (X1), 4=forward (X2)
    if (n === 0) return 0x01; // VK_LBUTTON
    if (n === 1) return 0x04; // VK_MBUTTON
    if (n === 2) return 0x02; // VK_RBUTTON
    if (n === 3) return 0x05; // VK_XBUTTON1 (back)
    if (n === 4) return 0x06; // VK_XBUTTON2 (forward)
    return null;
  }
  // Direct e.code to VK mapping for non-Key/non-Mouse codes
  const codeMap: Record<string, number> = {
    BracketLeft: 0xdb,   // VK_OEM_4 [
    BracketRight: 0xdd,  // VK_OEM_6 ]
    Backslash: 0xdc,     // VK_OEM_5 \.
    Semicolon: 0xba,     // VK_OEM_1 ;
    Quote: 0xde,         // VK_OEM_7 '
    Comma: 0xbc,         // VK_OEM_COMMA ,
    Period: 0xbe,        // VK_OEM_PERIOD .
    Slash: 0xbf,         // VK_OEM_2 /
    Backquote: 0xc0,     // VK_OEM_3 `
    Minus: 0xbd,         // VK_OEM_MINUS -
    Equal: 0xbb,         // VK_OEM_PLUS =
    Space: 0x20,
    Enter: 0x0d,
    Tab: 0x09,
    Escape: 0x1b,
    Backspace: 0x08,
    ShiftLeft: 0xa0,
    ShiftRight: 0xa1,
    ControlLeft: 0xa2,
    ControlRight: 0xa3,
    AltLeft: 0xa4,
    AltRight: 0xa5,
    CapsLock: 0x14,
    ArrowLeft: 0x25,
    ArrowUp: 0x26,
    ArrowRight: 0x27,
    ArrowDown: 0x28,
    Home: 0x24,
    End: 0x23,
    PageUp: 0x21,
    PageDown: 0x22,
    Insert: 0x2d,
    Delete: 0x2e
  };
  if (s in codeMap) return codeMap[s];
  if (s.startsWith('Key')) {
    const key = s.slice(3);
    if (key.length === 1) {
      const upper = key.toUpperCase();
      if (upper >= 'A' && upper <= 'Z') return upper.charCodeAt(0); // 0x41-0x5A
    }
  }
  if (s.startsWith('Digit') && s.length === 6) return 0x30 + parseInt(s.slice(5), 10);
  if (s.startsWith('Numpad')) {
    const num = parseInt(s.slice(6), 10);
    if (num >= 0 && num <= 9) return 0x60 + num; // VK_NUMPAD0-9
  }
  if (s.startsWith('F') && s.length >= 2 && s.length <= 3) {
    const n = parseInt(s.slice(1), 10);
    if (n >= 1 && n <= 12) return 0x70 + (n - 1);
  }
  return null;
}

let loaded = false;
let getAsyncKeyState: (vk: number) => number = () => 0;

function ensureLoaded(): boolean {
  if (loaded) return true;
  try {
    const koffi = require('koffi');
    const user32 = koffi.load('user32.dll');
    // GetAsyncKeyState(int vKey) -> SHORT. High bit (0x8000) = key is currently down.
    getAsyncKeyState = user32.func('int __stdcall GetAsyncKeyState(int)');
    loaded = true;
    return true;
  } catch {
    return false;
  }
}

/**
 * Start polling the given VK. When key state changes, calls onState(pressed).
 * Returns a function to stop polling.
 */
export function startPttBackgroundPoll(vk: number, onState: (pressed: boolean) => void): () => void {
  if (!ensureLoaded()) return () => {};
  const DOWN = 0x8000;
  let lastPressed: boolean | null = null;
  const id = setInterval(() => {
    try {
      const state = getAsyncKeyState(vk);
      const pressed = (state & DOWN) !== 0;
      if (lastPressed !== pressed) {
        lastPressed = pressed;
        onState(pressed);
      }
    } catch {
      /* ignore */
    }
  }, POLL_MS);
  return () => {
    clearInterval(id);
  };
}
