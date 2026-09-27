#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cstdint>

// ── Portable P-256 ECDSA verification ─────────────────────────────────────
//
// Licence proofs are signed with ECDSA over P-256. Windows verifies them with
// BCrypt and macOS with Security.framework, but Android has neither, and the
// verifier used to refuse every proof there - so a key-protected Android build
// would have locked out every paying customer. This is the verifier for those
// platforms.
//
// Verification only: public inputs, no secrets, so constant-time arithmetic is
// not needed. It refuses on any doubt - malformed key, point off the curve,
// r or s out of range, point at infinity - and never accepts by default.
//
// Fixed 256-bit numbers with Montgomery multiplication. juce::BigInteger was
// tried first and is far too slow for this: it multiplies and divides a bit at
// a time, and a single verification never finished. It is proved in tools/test
// against real signatures, since no Android device is available.

namespace amanorsacstudio
{
namespace p256
{

// Little-endian 32-bit limbs.
struct U256 { uint32_t w[8] {}; };

inline U256 fromBytes (const uint8_t* b)   // 32 bytes, big-endian
{
    U256 r;
    for (int i = 0; i < 8; ++i)
    {
        const auto* o = b + 28 - 4 * i;
        r.w[i] = (uint32_t) o[0] << 24 | (uint32_t) o[1] << 16 | (uint32_t) o[2] << 8 | (uint32_t) o[3];
    }
    return r;
}

inline U256 fromLimbs (uint32_t w7, uint32_t w6, uint32_t w5, uint32_t w4,
                       uint32_t w3, uint32_t w2, uint32_t w1, uint32_t w0)
{
    U256 r;
    r.w[0] = w0; r.w[1] = w1; r.w[2] = w2; r.w[3] = w3;
    r.w[4] = w4; r.w[5] = w5; r.w[6] = w6; r.w[7] = w7;
    return r;
}

inline bool isZero (const U256& a)
{
    uint32_t any = 0;
    for (auto x : a.w) any |= x;
    return any == 0;
}

inline int cmp (const U256& a, const U256& b)
{
    for (int i = 7; i >= 0; --i)
        if (a.w[i] != b.w[i]) return a.w[i] < b.w[i] ? -1 : 1;
    return 0;
}

inline bool bit (const U256& a, int i) { return ((a.w[i / 32] >> (i % 32)) & 1u) != 0; }

inline uint32_t addTo (U256& r, const U256& a, const U256& b)   // returns carry
{
    uint64_t c = 0;
    for (int i = 0; i < 8; ++i) { c += (uint64_t) a.w[i] + b.w[i]; r.w[i] = (uint32_t) c; c >>= 32; }
    return (uint32_t) c;
}

inline uint32_t subFrom (U256& r, const U256& a, const U256& b)  // returns borrow
{
    int64_t c = 0;
    for (int i = 0; i < 8; ++i)
    {
        c += (int64_t) a.w[i] - (int64_t) b.w[i];
        r.w[i] = (uint32_t) c;
        c = c < 0 ? -1 : 0;
    }
    return c < 0 ? 1u : 0u;
}

// Arithmetic modulo an odd prime, in Montgomery form (R = 2^256).
struct Field
{
    U256 m;
    uint32_t minv = 0;   // -m^-1 mod 2^32
    U256 r2;             // R^2 mod m
    U256 one;            // R mod m, i.e. 1 in Montgomery form

    explicit Field (const U256& modulus) : m (modulus)
    {
        uint32_t inv = 1;
        for (int i = 0; i < 5; ++i) inv *= 2u - m.w[0] * inv;
        minv = (uint32_t) (0u - inv);

        U256 x; x.w[0] = 1;
        for (int i = 0; i < 256; ++i) x = add (x, x);
        one = x;
        for (int i = 0; i < 256; ++i) x = add (x, x);
        r2 = x;
    }

    U256 add (const U256& a, const U256& b) const
    {
        U256 r;
        const auto carry = addTo (r, a, b);
        if (carry != 0 || cmp (r, m) >= 0) subFrom (r, r, m);
        return r;
    }

    U256 sub (const U256& a, const U256& b) const
    {
        U256 r;
        if (subFrom (r, a, b) != 0) addTo (r, r, m);
        return r;
    }

    U256 mul (const U256& a, const U256& b) const   // Montgomery product, CIOS
    {
        uint32_t t[10] = {};
        for (int i = 0; i < 8; ++i)
        {
            uint64_t c = 0;
            for (int j = 0; j < 8; ++j)
            {
                const uint64_t s = (uint64_t) t[j] + (uint64_t) a.w[j] * b.w[i] + c;
                t[j] = (uint32_t) s; c = s >> 32;
            }
            uint64_t s = (uint64_t) t[8] + c;
            t[8] = (uint32_t) s; t[9] = (uint32_t) (s >> 32);

            const uint32_t q = t[0] * minv;
            s = (uint64_t) t[0] + (uint64_t) q * m.w[0];
            c = s >> 32;
            for (int j = 1; j < 8; ++j)
            {
                s = (uint64_t) t[j] + (uint64_t) q * m.w[j] + c;
                t[j - 1] = (uint32_t) s; c = s >> 32;
            }
            s = (uint64_t) t[8] + c;
            t[7] = (uint32_t) s;
            t[8] = t[9] + (uint32_t) (s >> 32);
            t[9] = 0;
        }

        U256 r;
        for (int i = 0; i < 8; ++i) r.w[i] = t[i];
        if (t[8] != 0 || cmp (r, m) >= 0) subFrom (r, r, m);
        return r;
    }

    U256 toMont   (const U256& a) const { return mul (a, r2); }
    U256 fromMont (const U256& a) const { U256 o; o.w[0] = 1; return mul (a, o); }

    U256 inverse (const U256& aMont) const   // a^(m-2), m prime
    {
        U256 two; two.w[0] = 2;
        U256 e; subFrom (e, m, two);
        U256 r = one;
        for (int i = 255; i >= 0; --i)
        {
            r = mul (r, r);
            if (bit (e, i)) r = mul (r, aMont);
        }
        return r;
    }
};

inline const Field& fieldP()
{
    static const Field f (fromLimbs (0xffffffff, 0x00000001, 0x00000000, 0x00000000,
                                     0x00000000, 0xffffffff, 0xffffffff, 0xffffffff));
    return f;
}

inline const Field& fieldN()
{
    static const Field f (fromLimbs (0xffffffff, 0x00000000, 0xffffffff, 0xffffffff,
                                     0xbce6faad, 0xa7179e84, 0xf3b9cac2, 0xfc632551));
    return f;
}

// A point in Jacobian coordinates, Montgomery form: affine (X / Z^2, Y / Z^3).
struct Point
{
    U256 x, y, z;
    bool infinity = true;
};

// dbl-2001-b, valid because a = -3.
inline Point dbl (const Point& pt)
{
    if (pt.infinity || isZero (pt.y))
        return {};

    const auto& F = fieldP();
    const auto delta = F.mul (pt.z, pt.z);
    const auto gamma = F.mul (pt.y, pt.y);
    const auto beta  = F.mul (pt.x, gamma);
    const auto t     = F.mul (F.sub (pt.x, delta), F.add (pt.x, delta));
    const auto alpha = F.add (F.add (t, t), t);
    const auto beta2 = F.add (beta, beta);
    const auto beta4 = F.add (beta2, beta2);
    const auto beta8 = F.add (beta4, beta4);

    Point r;
    r.infinity = false;
    r.x = F.sub (F.mul (alpha, alpha), beta8);
    const auto yz = F.add (pt.y, pt.z);
    r.z = F.sub (F.sub (F.mul (yz, yz), gamma), delta);
    const auto g2  = F.mul (gamma, gamma);
    const auto g4  = F.add (g2, g2);
    const auto g8a = F.add (g4, g4);
    const auto g8  = F.add (g8a, g8a);
    r.y = F.sub (F.mul (alpha, F.sub (beta4, r.x)), g8);
    return r;
}

// add-2007-bl.
inline Point add (const Point& a, const Point& b)
{
    if (a.infinity) return b;
    if (b.infinity) return a;

    const auto& F = fieldP();
    const auto z1z1 = F.mul (a.z, a.z);
    const auto z2z2 = F.mul (b.z, b.z);
    const auto u1 = F.mul (a.x, z2z2);
    const auto u2 = F.mul (b.x, z1z1);
    const auto s1 = F.mul (F.mul (a.y, b.z), z2z2);
    const auto s2 = F.mul (F.mul (b.y, a.z), z1z1);
    const auto h  = F.sub (u2, u1);
    const auto sd = F.sub (s2, s1);
    const auto rr = F.add (sd, sd);

    if (isZero (h))
        return isZero (rr) ? dbl (a) : Point {};

    const auto h2 = F.add (h, h);
    const auto i  = F.mul (h2, h2);
    const auto j  = F.mul (h, i);
    const auto v  = F.mul (u1, i);

    Point r;
    r.infinity = false;
    r.x = F.sub (F.sub (F.mul (rr, rr), j), F.add (v, v));
    const auto s1j = F.mul (s1, j);
    r.y = F.sub (F.mul (rr, F.sub (v, r.x)), F.add (s1j, s1j));
    const auto zs = F.add (a.z, b.z);
    r.z = F.mul (F.sub (F.sub (F.mul (zs, zs), z1z1), z2z2), h);
    return r;
}

/**
 * Verifies an ECDSA P-256 signature over SHA-256(message).
 *
 * publicKey65  SEC1 uncompressed point: 0x04 || X || Y
 * signature64  IEEE P1363: r || s, each 32 bytes big-endian
 */
inline bool verify (const uint8_t* publicKey65, const void* message, size_t messageBytes,
                    const uint8_t* signature64)
{
    if (publicKey65 == nullptr || signature64 == nullptr || (message == nullptr && messageBytes > 0))
        return false;
    if (publicKey65[0] != 0x04)
        return false;

    const auto& P = fieldP();
    const auto& N = fieldN();

    const auto qx = fromBytes (publicKey65 + 1);
    const auto qy = fromBytes (publicKey65 + 33);
    if (cmp (qx, P.m) >= 0 || cmp (qy, P.m) >= 0)
        return false;

    // The key must lie on the curve: y^2 = x^3 - 3x + b. A point off it could
    // be chosen to make a forged signature pass.
    const auto b = P.toMont (fromLimbs (0x5ac635d8, 0xaa3a93e7, 0xb3ebbd55, 0x769886bc,
                                        0x651d06b0, 0xcc53b0f6, 0x3bce3c3e, 0x27d2604b));
    const auto xM = P.toMont (qx);
    const auto yM = P.toMont (qy);
    const auto x3 = P.mul (P.mul (xM, xM), xM);
    const auto threeX = P.add (P.add (xM, xM), xM);
    if (cmp (P.mul (yM, yM), P.add (P.sub (x3, threeX), b)) != 0)
        return false;

    const auto r = fromBytes (signature64);
    const auto s = fromBytes (signature64 + 32);
    if (isZero (r) || isZero (s) || cmp (r, N.m) >= 0 || cmp (s, N.m) >= 0)
        return false;

    const uint8_t zeros = 0;
    const juce::SHA256 digest (message != nullptr ? message : &zeros, messageBytes);
    const auto hash = digest.getRawData();
    auto e = fromBytes (static_cast<const uint8_t*> (hash.getData()));
    if (cmp (e, N.m) >= 0) subFrom (e, e, N.m);

    const auto w  = N.inverse (N.toMont (s));
    const auto u1 = N.fromMont (N.mul (N.toMont (e), w));
    const auto u2 = N.fromMont (N.mul (N.toMont (r), w));

    Point g;
    g.infinity = false;
    g.x = P.toMont (fromLimbs (0x6b17d1f2, 0xe12c4247, 0xf8bce6e5, 0x63a440f2,
                               0x77037d81, 0x2deb33a0, 0xf4a13945, 0xd898c296));
    g.y = P.toMont (fromLimbs (0x4fe342e2, 0xfe1a7f9b, 0x8ee7eb4a, 0x7c0f9e16,
                               0x2bce3357, 0x6b315ece, 0xcbb64068, 0x37bf51f5));
    g.z = P.one;

    Point q;
    q.infinity = false;
    q.x = xM; q.y = yM; q.z = P.one;

    const auto gq = add (g, q);

    // u1*G + u2*Q in one pass (Shamir's trick).
    Point acc;
    for (int i = 255; i >= 0; --i)
    {
        acc = dbl (acc);
        const bool b1 = bit (u1, i), b2 = bit (u2, i);
        if (b1 && b2)  acc = add (acc, gq);
        else if (b1)   acc = add (acc, g);
        else if (b2)   acc = add (acc, q);
    }

    if (acc.infinity)
        return false;

    const auto zinv = P.inverse (acc.z);
    auto x = P.fromMont (P.mul (acc.x, P.mul (zinv, zinv)));
    if (cmp (x, N.m) >= 0) subFrom (x, x, N.m);

    return cmp (x, r) == 0;
}

} // namespace p256
} // namespace amanorsacstudio
