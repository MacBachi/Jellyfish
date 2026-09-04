// P-224 (secp224r1) in plain JavaScript with BigInt, for OpenHaystack keys. No library, no
// Web Crypto (browsers do not offer P-224 there). Randomness comes from crypto.getRandomValues.
//
// Output format follows the OpenHaystack reference (generate_keys.py): the private key is
// the 28-byte big-endian scalar, the advertisement key the 28-byte big-endian x coordinate of
// the public point, the hashed key SHA-256 of that x, all base64.
(function (root) {
  'use strict';
  const P = (1n << 224n) - (1n << 96n) + 1n;
  const A = P - 3n;
  const B = 0xb4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4n;
  const N = 0xffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3dn;
  const GX = 0xb70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21n;
  const GY = 0xbd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34n;

  const mod = (a, m) => { const r = a % m; return r < 0n ? r + m : r; };
  function modpow(b, e, m) { let r = 1n; b = mod(b, m); while (e > 0n) { if (e & 1n) r = r * b % m; b = b * b % m; e >>= 1n; } return r; }
  const inv = (a) => modpow(a, P - 2n, P); // P is prime

  // Jacobian coordinates: [X, Y, Z], the point at infinity has Z = 0.
  const INF = [0n, 1n, 0n];
  function dbl([X1, Y1, Z1]) {
    if (Z1 === 0n) return INF;
    const delta = Z1 * Z1 % P, gamma = Y1 * Y1 % P, beta = X1 * gamma % P;
    const alpha = 3n * mod((X1 - delta) * (X1 + delta), P) % P;
    const X3 = mod(alpha * alpha - 8n * beta, P);
    const Z3 = mod((Y1 + Z1) * (Y1 + Z1) - gamma - delta, P);
    const Y3 = mod(alpha * mod(4n * beta - X3, P) - 8n * gamma * gamma, P);
    return [X3, Y3, Z3];
  }
  function add(p1, p2) {
    const [X1, Y1, Z1] = p1, [X2, Y2, Z2] = p2;
    if (Z1 === 0n) return p2;
    if (Z2 === 0n) return p1;
    const Z1Z1 = Z1 * Z1 % P, Z2Z2 = Z2 * Z2 % P;
    const U1 = X1 * Z2Z2 % P, U2 = X2 * Z1Z1 % P;
    const S1 = Y1 * Z2 % P * Z2Z2 % P, S2 = Y2 * Z1 % P * Z1Z1 % P;
    const H = mod(U2 - U1, P), r = mod(2n * (S2 - S1), P);
    if (H === 0n) return r === 0n ? dbl(p1) : INF;
    const I = 4n * H * H % P, J = H * I % P, V = U1 * I % P;
    const X3 = mod(r * r - J - 2n * V, P);
    const Y3 = mod(r * mod(V - X3, P) - 2n * S1 * J, P);
    const Z3 = mod(((Z1 + Z2) * (Z1 + Z2) - Z1Z1 - Z2Z2) * H, P);
    return [X3, Y3, Z3];
  }
  function mul(k, point) {
    let acc = INF, q = point;
    while (k > 0n) { if (k & 1n) acc = add(acc, q); q = dbl(q); k >>= 1n; }
    return acc;
  }
  function affine([X, Y, Z]) {
    if (Z === 0n) return null;
    const zi = inv(Z), zi2 = zi * zi % P;
    return [X * zi2 % P, Y * zi2 % P * zi % P];
  }
  const onCurve = ([x, y]) => mod(y * y - (x * x * x + A * x + B), P) === 0n;

  function bytesToBig(bytes) { let v = 0n; for (const b of bytes) v = (v << 8n) | BigInt(b); return v; }
  function bigToBytes(v, len) { const out = new Uint8Array(len); for (let i = len - 1; i >= 0; i--) { out[i] = Number(v & 0xffn); v >>= 8n; } return out; }
  function b64(bytes) {
    if (typeof btoa === 'function') { let s = ''; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); }
    return Buffer.from(bytes).toString('base64');
  }

  // SHA-256, for the hashed advertisement key (works on file:// where crypto.subtle may not).
  function sha256(bytes) {
    const K = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
    const H = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    const l = bytes.length, padded = new Uint8Array(((l + 9 + 63) >> 6) << 6);
    padded.set(bytes); padded[l] = 0x80;
    const bits = l * 8; padded[padded.length - 4] = (bits >>> 24) & 255; padded[padded.length - 3] = (bits >>> 16) & 255; padded[padded.length - 2] = (bits >>> 8) & 255; padded[padded.length - 1] = bits & 255;
    const w = new Uint32Array(64), rotr = (x, n) => (x >>> n) | (x << (32 - n));
    for (let off = 0; off < padded.length; off += 64) {
      for (let i = 0; i < 16; i++) w[i] = (padded[off + i * 4] << 24) | (padded[off + i * 4 + 1] << 16) | (padded[off + i * 4 + 2] << 8) | padded[off + i * 4 + 3];
      for (let i = 16; i < 64; i++) { const s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >>> 3), s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >>> 10); w[i] = (w[i - 16] + s0 + w[i - 7] + s1) | 0; }
      let [a, b, c, d, e, f, g, h] = H;
      for (let i = 0; i < 64; i++) {
        const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g), t1 = (h + S1 + ch + K[i] + w[i]) | 0;
        const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = (S0 + mj) | 0;
        h = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
      }
      H[0] = (H[0] + a) | 0; H[1] = (H[1] + b) | 0; H[2] = (H[2] + c) | 0; H[3] = (H[3] + d) | 0; H[4] = (H[4] + e) | 0; H[5] = (H[5] + f) | 0; H[6] = (H[6] + g) | 0; H[7] = (H[7] + h) | 0;
    }
    const out = new Uint8Array(32);
    for (let i = 0; i < 8; i++) { out[i * 4] = H[i] >>> 24; out[i * 4 + 1] = (H[i] >>> 16) & 255; out[i * 4 + 2] = (H[i] >>> 8) & 255; out[i * 4 + 3] = H[i] & 255; }
    return out;
  }

  // The public x coordinate for a private scalar; throws if the scalar is out of range.
  function publicX(priv) {
    if (priv < 1n || priv >= N) throw new Error('private key out of range');
    const pt = affine(mul(priv, [GX, GY, 1n]));
    if (!pt || !onCurve(pt)) throw new Error('point not on curve');
    return pt[0];
  }

  function randomScalar() {
    const buf = new Uint8Array(28);
    for (;;) {
      if (root.crypto && root.crypto.getRandomValues) root.crypto.getRandomValues(buf);
      else require('crypto').randomFillSync(buf);
      const d = bytesToBig(buf);
      if (d >= 1n && d < N) return d;
    }
  }

  // One key pair in the OpenHaystack format.
  function generate(priv) {
    const d = priv === undefined ? randomScalar() : priv;
    const x = publicX(d);
    const privBytes = bigToBytes(d, 28), advBytes = bigToBytes(x, 28);
    return {
      privateKey: b64(privBytes),
      advertisementKey: b64(advBytes),
      hashedKey: b64(sha256(advBytes)),
      privateBytes: privBytes,
      advertisementBytes: advBytes,
    };
  }

  // Sanity checks the page runs before offering keys: G on the curve, n·G = infinity.
  function selfTest() {
    if (!onCurve([GX, GY])) return 'generator not on curve';
    if (affine(mul(N, [GX, GY, 1n])) !== null) return 'order check failed';
    const t = generate(1n);
    if (t.advertisementBytes.some((b, i) => b !== bigToBytes(GX, 28)[i])) return 'k=1 mismatch';
    return null;
  }

  root.P224 = { generate, publicX, selfTest, sha256, N, bytesToBig, bigToBytes, b64 };
})(typeof globalThis !== 'undefined' ? globalThis : this);
if (typeof module !== 'undefined') module.exports = globalThis.P224;
