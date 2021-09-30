#ifndef bqtNewhashHH
#define bqtNewhashHH

#ifdef __cplusplus
extern "C" {
#endif

#include "endian.hh"

typedef std::uint_least32_t newhash_t;

extern newhash_t newhash_calc(const unsigned char* buf, unsigned long size);
extern newhash_t newhash_calc_upd(newhash_t c, const unsigned char* buf, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif
