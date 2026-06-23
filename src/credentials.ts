export interface CredentialCrypto {
  encrypt(text: string): string;
  decrypt(cipher: string): string;
}

export interface CredentialStore {
  get(key: string, defaultValue?: string): string;
  set(key: string, value: string): void;
}

export interface SavedServerLike {
  id: string;
  url: string;
  name?: string;
  icon?: string;
  keepConnected?: boolean;
  identity?: string;
  password?: string;
}

function originOf(url: string): string | null {
  try {
    return new URL(url).origin;
  } catch {
    return null;
  }
}

export function findServerByOrigin(
  servers: SavedServerLike[],
  origin: string
): SavedServerLike | undefined {
  return servers.find((s) => originOf(s.url) === origin);
}

export function saveCredentials(
  servers: SavedServerLike[],
  crypto: CredentialCrypto,
  origin: string,
  identity: string,
  password: string
): SavedServerLike[] {
  const idx = servers.findIndex((s) => originOf(s.url) === origin);
  if (idx === -1) return servers; // never auto-create a server entry
  const next = servers.slice();
  next[idx] = {
    ...servers[idx],
    identity,
    password: crypto.encrypt(password)
  };
  return next;
}

export function loadCredentials(
  servers: SavedServerLike[],
  crypto: CredentialCrypto,
  origin: string
): { identity: string; password: string } | null {
  const srv = findServerByOrigin(servers, origin);
  if (!srv || !srv.identity || !srv.password) return null;
  return { identity: srv.identity, password: crypto.decrypt(srv.password) };
}

export function clearCredentials(
  servers: SavedServerLike[],
  origin: string
): SavedServerLike[] {
  const idx = servers.findIndex((s) => originOf(s.url) === origin);
  if (idx === -1) return servers;
  const next = servers.slice();
  const { identity: _i, password: _p, ...rest } = servers[idx];
  next[idx] = rest;
  return next;
}
