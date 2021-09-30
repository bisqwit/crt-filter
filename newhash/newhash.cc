#include "endian.hh"
#include "newhash.hh"
#include <algorithm>

#include "simd.hh"

#ifdef __GNUC__
# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif

/* Note: The differing algorithms in this
 *       file are not value-compatible.
 */

#if defined(__x86_64) || (defined(USE_MMX) && defined(__SSE2__))
/* On x86_64, we can use 64-bit registers. Which is fast.
 * On 32-bit, we can use MMX registers, if MMX is enabled
 *   However, SSE2 must ALSO be enabled, because otherwise
 *   we cannot do 64-bit sub/add efficiently.
 */
# define SIXTY_BIT_PLATFORM
#else
# undef SIXTY_BIT_PLATFORM
#endif

#if 0 && defined(__GNUC__) && defined(__LP64__) && !defined(__ICC)
# define HUNDREDTWENTYEIGHTBIT_PLATFORM
/*
 * 128-bit with SSE2 is not feasible, because SSE2 does not
 * have 128-bit add/sub ops. They cannot be even synthesized
 * from 64-bit adds/subs, because there's no carry update.
 * More importantly there's no 128-bit shift to left or right.
 *
 * __attribute__((mode(TI))) can be used to created a 128-bit
 * integer type on GCC, however, it does not work on ICC.
 */

#ifdef __SSE2__
 #include <xmmintrin.h>
#endif

#else
# undef HUNDREDTWENTYEIGHTBIT_PLATFORM
#endif

/* Based on Robert J. Jenkins Jr.'s "zobra" hash code
 * References:
 *   http://www.burtleburtle.net/bob/hash/evahash.html
 *   http://www.cris.com/~Ttwang/tech/inthash.htm
 *
 * Copyright (C) 2011 Joel Yliluoma (http://iki.fi/bisqwit/)
 */

template<typename T>
static inline T rol(T v, int n) { return (v<<n) | (v>>int( sizeof(T)*8 - n)); }

/* The mixing step */
#define mix32z(a,b,c)  \
do{ \
  a=(a-c) ^ rol(c,16); c += b; \
  b=(b-a) ^ rol(a,23); a += c; \
  c=(c-b) ^ rol(b,29); b += a; \
  a=(a-c) ^ rol(c,16); c += b; \
  b=(b-a) ^ rol(a,19); a += c; \
  c=(c-b) ^ rol(b,17); b += a; \
}while(0)
#define final32z(a,b,c)  \
do{ \
  c=(c^b) - rol(b, 5); \
  a=(a^c) - rol(c,10); \
  b=(b^a) - rol(a, 6); \
  c=(c^b) - rol(b, 9); \
}while(0)

#define mix64z(a,b,c)  \
do{ \
  a=(a-c) ^ rol(c, 2); c += b; \
  b=(b-a) ^ rol(a,22); a += c; \
  c=(c-b) ^ rol(b, 3); b += a; \
  a=(a-c) ^ rol(c,36); c += b; \
  b=(b-a) ^ rol(a,48); a += c; \
  c=(c-b) ^ rol(b,42); b += a; \
}while(0)
#define final64z(a,b,c)  \
do{ \
  c=(c^b) - rol(b,22); \
  a=(a^c) - rol(c, 3); \
  b=(b^a) - rol(a,58); \
  c=(c^b) - rol(b,48); \
}while(0)

#define mix128z(a,b,c)  \
do{ \
  a=(a-c) ^ rol(c, 79); c += b; \
  b=(b-a) ^ rol(a,124); a += c; \
  c=(c-b) ^ rol(b, 60); b += a; \
  a=(a-c) ^ rol(c, 74); c += b; \
  b=(b-a) ^ rol(a,115); a += c; \
  c=(c-b) ^ rol(b,101); b += a; \
}while(0)
#define final128z(a,b,c)  \
do{ \
  c=(c^b) - rol(b, 60); \
  a=(a^c) - rol(c, 20); \
  b=(b^a) - rol(a, 91); \
  c=(c^b) - rol(b,106); \
}while(0)


#ifdef HUNDREDTWENTYEIGHTBIT_PLATFORM
typedef unsigned int std::uint128_t __attribute__((mode(TI)));

class c128
{
public:
    std::uint128_t value;
public:
    c128() : value()
    {
    }
    c128(std::uint128_t v) : value(v) { }
    c128(std::uint_fast64_t a, std::uint_fast64_t b)
        : value(a)
    {
        value <<= 64;
        value |= b;
    }
    c128(std::uint_least64_t a) : value(a)
    {
    }
    c128(std::uint_least32_t a) : value(a)
    {
    }
    #ifdef __SSE2__
    c128(const __m128& b) : value(*(const std::uint128_t*)&b)
    {
    }
    #endif

    c128& operator += (const c128& b) { value += b.value; return *this; }
    c128& operator -= (const c128& b) { value -= b.value; return *this; }
    c128& operator ^= (const c128& b)
    {
    #ifdef __SSE2__
        *(__m128*)&value = _mm_xor_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        value ^= b.value;
    #endif
        return *this;
    }
    c128& operator &= (const c128& b)
    {
    #ifdef __SSE2__
        *(__m128*)&value = _mm_and_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        value &= b.value;
    #endif
        return *this;
    }
    c128& operator |= (const c128& b)
    {
    #ifdef __SSE2__
        *(__m128*)&value = _mm_or_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        value |= b.value;
    #endif
        return *this;
    }
    c128& operator <<= (int nbits) { value <<= nbits; return *this; }
    c128& operator >>= (int nbits) { value >>= nbits; return *this; }

    c128 operator+ (const c128& b) const { return value + b.value; }
    c128 operator- (const c128& b) const { return value - b.value; }
    c128 operator^ (const c128& b) const
    {
    #ifdef __SSE2__
        return _mm_xor_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        return value ^ b.value;
    #endif
    }
    c128 operator& (const c128& b) const
    {
    #ifdef __SSE2__
        return _mm_and_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        return value & b.value;
    #endif
    }
    c128 operator| (const c128& b) const
    {
    #ifdef __SSE2__
        return _mm_or_ps( *(const __m128*)&value, *(const __m128*)&b.value);
    #else
        return value | b.value;
    #endif
    }
    c128 operator<< (int nbits) const { return value << nbits; }
    c128 operator>> (int nbits) const { return value >> nbits; }
    c128 operator~ () const { return ~value; }
};

c128 get_128(const void* p)
{
  #ifdef LITTLE_ENDIAN_AND_UNALIGNED_ACCESS_OK
    return *(const std::uint128_t*)p;
  #else
    const unsigned char* data = (const unsigned char*)p;
    c128 res( get_64(data) );
    c128 res2( get_64(data + 8) );
    res |= res2 << 64;
    return res;
  #endif
}
static inline c128 RnSubstitute(const void* p, unsigned bytes)
{
    const unsigned char* data = (const unsigned char*)p;
    switch(bytes)
    {
        case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8:
            return get_n(p, bytes);
        case 16: return get_128(p);
    }
    return c128(get_64(data)) | (c128(get_n(data+8, bytes-8)) << 64);
}
#define Rn RnSubstitute

#endif // 128bit

newhash_t newhash_calc(const unsigned char* buf, unsigned long size)
{
    return newhash_calc_upd(0, buf, size);
}
newhash_t newhash_calc_upd(newhash_t c, const unsigned char* buf, unsigned long size)
{
#ifdef HUNDREDTWENTYEIGHTBIT_PLATFORM
    c128 c_cast = c; {
    unsigned long len = size;
    c128 a(UINT64_C(0x9e3779b97f4a7c15),UINT64_C(0xf39cc0605cedc834)); // 2^128 / ((1+sqrt(5))/2)
    a += c_cast + size;
    c128 b(a), c(a);
    while(len >= 16*3)
    {
        a += (c128)get_128(buf+0);
        b += (c128)get_128(buf+16);
        c += (c128)get_128(buf+32);
        mix128z(a,b,c);
        buf += 48; len -= 48;
    }
    /*------------------------------------- handle the last 47 bytes */
    if(len > 0)
    {
        if(len >= 32)      { a += (c128)get_128(buf); b += (c128)get_128(buf+16); c += (c128)get_n(buf+32,len-32); }
        else if(len >= 16) { a += (c128)get_128(buf); b += (c128)get_n(buf+16, len-16); }
        else               { a += (c128)get_n(buf, len); }
        final128z(a,b,c);
    }
    /*-------------------------------------------- report the result */
    return c.value; /* Note: this returns just the lowest 32 bits of the hash */
   }
#elif defined(SIXTY_BIT_PLATFORM)
    c64 c_cast = (std::uint_fast64_t)c; {
    unsigned long len = size;
    c64 a(UINT64_C(0x9e3779b97f4a7c13)); // 2^64 / ((1+sqrt(5))/2)
    a += c_cast + c64(std::uint64_t(size));
    c64 b(a), c(a);
    while(len >= 8*3)
    {
        a += (c64)get_64(buf+0);
        b += (c64)get_64(buf+8);
        c += (c64)get_64(buf+16);
        mix64z(a,b,c);
        buf += 24; len -= 24;
    }
    /*------------------------------------- handle the last 23 bytes */
    if(len > 0)
    {
        if(len >= 16)     { a += (c64)get_64(buf); b += (c64)get_64(buf+8); c += (c64)get_n(buf+16,len-16); }
        else if(len >= 8) { a += (c64)get_64(buf); b += (c64)get_n(buf+8, len-8); }
        else              { a += (c64)get_n(buf, len); }
        final64z(a,b,c);
    }
    /*-------------------------------------------- report the result */
  #ifdef USE_MMX
    newhash_t result = get_32(&c.value); /* Note: this returns just the lowest 32 bits of the hash */
    MMX_clear();
    return result;
  #else
    return c.value; /* Note: this returns just the lowest 32 bits of the hash */
  #endif
   }
#else
    typedef std::uint_least32_t c32;
    c32 a,b;
    unsigned long len = size;
    c += size + UINT32_C(0x9e3779b9); // 2^32 / ((1+sqrt(5))/2
    a = b = c;
    while(len >= 4*3)
    {
        a += get_32(buf+0);
        b += get_32(buf+4);
        c += get_32(buf+8);
        mix32z(a,b,c);
        buf += 12; len -= 12;
    }
    /*------------------------------------- handle the last 11 bytes */
    if(len > 0)
    {
        if(len >= 8)      { a += (c32)get_32(buf); b += (c32)get_32(buf+4); c += (c32)get_n(buf+8,len-8); }
        else if(len >= 4) { a += (c32)get_32(buf); b += (c32)get_n(buf+4, len-4); }
        else              { a += (c32)get_n(buf, len); }
        final32z(a,b,c);
    }
    /*-------------------------------------------- report the result */
    return c;
#endif
}
