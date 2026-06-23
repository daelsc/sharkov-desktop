import { describe, it, expect } from 'vitest';
import {
  findServerByOrigin,
  saveCredentials,
  loadCredentials,
  clearCredentials,
  type SavedServerLike,
  type CredentialCrypto
} from '../src/credentials';

// Deterministic fake crypto: reverse the string as "encryption".
const fakeCrypto: CredentialCrypto = {
  encrypt: (t) => t.split('').reverse().join(''),
  decrypt: (c) => c.split('').reverse().join('')
};

const servers: SavedServerLike[] = [
  { id: 'a', url: 'https://chat.example.com' },
  { id: 'b', url: 'https://demo.sharkord.com' }
];

describe('findServerByOrigin', () => {
  it('matches by origin', () => {
    expect(findServerByOrigin(servers, 'https://chat.example.com')?.id).toBe('a');
  });
  it('returns undefined when no match', () => {
    expect(findServerByOrigin(servers, 'https://nope.example.com')).toBeUndefined();
  });
});

describe('saveCredentials', () => {
  it('encrypts the password and stores identity on the matching server', () => {
    const next = saveCredentials(servers, fakeCrypto, 'https://chat.example.com', 'alice', 'hunter2');
    const srv = next.find((s) => s.id === 'a')!;
    expect(srv.identity).toBe('alice');
    expect(srv.password).toBe('2retnuh'); // reversed
  });
  it('does not mutate the input array or its objects', () => {
    const next = saveCredentials(servers, fakeCrypto, 'https://chat.example.com', 'alice', 'hunter2');
    expect(next).not.toBe(servers);
    expect(servers[0].identity).toBeUndefined();
    expect(servers[0].password).toBeUndefined();
  });
  it('returns the array unchanged when no server matches the origin', () => {
    const next = saveCredentials(servers, fakeCrypto, 'https://nope.example.com', 'alice', 'hunter2');
    expect(next).toEqual(servers);
  });
});

describe('loadCredentials', () => {
  it('decrypts and returns stored credentials', () => {
    const saved = saveCredentials(servers, fakeCrypto, 'https://chat.example.com', 'alice', 'hunter2');
    const creds = loadCredentials(saved, fakeCrypto, 'https://chat.example.com');
    expect(creds).toEqual({ identity: 'alice', password: 'hunter2' });
  });
  it('returns null when no server matches', () => {
    expect(loadCredentials(servers, fakeCrypto, 'https://nope.example.com')).toBeNull();
  });
  it('returns null when identity or password is missing', () => {
    expect(loadCredentials(servers, fakeCrypto, 'https://demo.sharkord.com')).toBeNull();
  });
});

describe('clearCredentials', () => {
  it('removes identity and password from the matching server', () => {
    const saved = saveCredentials(servers, fakeCrypto, 'https://chat.example.com', 'alice', 'hunter2');
    const cleared = clearCredentials(saved, 'https://chat.example.com');
    const srv = cleared.find((s) => s.id === 'a')!;
    expect(srv.identity).toBeUndefined();
    expect(srv.password).toBeUndefined();
  });
  it('does not mutate the input', () => {
    const saved = saveCredentials(servers, fakeCrypto, 'https://chat.example.com', 'alice', 'hunter2');
    clearCredentials(saved, 'https://chat.example.com');
    expect(saved.find((s) => s.id === 'a')!.identity).toBe('alice');
  });
});
