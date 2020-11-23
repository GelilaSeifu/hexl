//*****************************************************************************
// Copyright 2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <immintrin.h>

#include <vector>

#include "intel-lattice/util/util.hpp"
#include "logging/logging.hpp"
#include "number-theory/number-theory.hpp"
#include "util/check.hpp"

namespace intel {
namespace lattice {

// Returns the unsigned 64-bit integer values in x as a vector
inline std::vector<uint64_t> ExtractValues(__m512i x) {
  __m256i x0 = _mm512_extracti64x4_epi64(x, 0);
  __m256i x1 = _mm512_extracti64x4_epi64(x, 1);

  std::vector<uint64_t> xs{static_cast<uint64_t>(_mm256_extract_epi64(x0, 0)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x0, 1)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x0, 2)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x0, 3)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x1, 0)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x1, 1)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x1, 2)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x1, 3))};

  return xs;
}

inline std::vector<int64_t> ExtractIntValues(__m512i x) {
  __m256i x0 = _mm512_extracti64x4_epi64(x, 0);
  __m256i x1 = _mm512_extracti64x4_epi64(x, 1);

  std::vector<int64_t> xs{static_cast<int64_t>(_mm256_extract_epi64(x0, 0)),
                          static_cast<int64_t>(_mm256_extract_epi64(x0, 1)),
                          static_cast<int64_t>(_mm256_extract_epi64(x0, 2)),
                          static_cast<int64_t>(_mm256_extract_epi64(x0, 3)),
                          static_cast<int64_t>(_mm256_extract_epi64(x1, 0)),
                          static_cast<int64_t>(_mm256_extract_epi64(x1, 1)),
                          static_cast<int64_t>(_mm256_extract_epi64(x1, 2)),
                          static_cast<int64_t>(_mm256_extract_epi64(x1, 3))};

  return xs;
}

// Returns the unsigned 64-bit integer values in x as a vector
inline std::vector<uint64_t> ExtractValues(__m256i x) {
  std::vector<uint64_t> xs{static_cast<uint64_t>(_mm256_extract_epi64(x, 0)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x, 1)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x, 2)),
                           static_cast<uint64_t>(_mm256_extract_epi64(x, 3))};

  return xs;
}

// Returns the 64-bit floating-point values in x as a vector
inline std::vector<double> ExtractValues(__m512d x) {
  std::vector<double> ret(8, 0);
  double* x_data = reinterpret_cast<double*>(&x);
  for (size_t i = 0; i < 8; ++i) {
    ret[i] = x_data[i];
  }
  return ret;
}

// Returns the 64-bit floating-point values in x as a vector
inline std::vector<double> ExtractValues(__m256d x) {
  std::vector<double> ret(4, 0);
  double* x_data = reinterpret_cast<double*>(&x);
  for (size_t i = 0; i < 4; ++i) {
    ret[i] = x_data[i];
  }
  return ret;
}

// Checks all 64-bit values in x are less than bound
// Returns true
inline void CheckBounds(__m512i x, uint64_t bound) {
  (void)x;      // Ignore unused parameter warning
  (void)bound;  // Ignore unused parameter warning
  LATTICE_CHECK_BOUNDS(ExtractValues(x).data(), 512 / 64, bound);
}

// Multiply packed unsigned BitShift-bit integers in each 64-bit element of x
// and y to form a 2*BitShift-bit intermediate result.
// Returns the high BitShift-bit unsigned integer from the intermediate result
template <int BitShift>
inline __m256i _mm256_il_mulhi_epi(__m256i x, __m256i y);

template <>
inline __m256i _mm256_il_mulhi_epi<64>(__m256i x, __m256i y) {
  // https://stackoverflow.com/questions/28807341/simd-signed-with-unsigned-multiplication-for-64-bit-64-bit-to-128-bit
  __m256i lomask = _mm256_set1_epi64x(0x00000000ffffffff);
  __m256i xh =
      _mm256_shuffle_epi32(x, (_MM_PERM_ENUM)0xB1);  // x0l, x0h, x1l, x1h
  __m256i yh =
      _mm256_shuffle_epi32(y, (_MM_PERM_ENUM)0xB1);  // y0l, y0h, y1l, y1h
  __m256i w0 = _mm256_mul_epu32(x, y);               // x0l*y0l, x1l*y1l
  __m256i w1 = _mm256_mul_epu32(x, yh);              // x0l*y0h, x1l*y1h
  __m256i w2 = _mm256_mul_epu32(xh, y);              // x0h*y0l, x1h*y0l
  __m256i w3 = _mm256_mul_epu32(xh, yh);             // x0h*y0h, x1h*y1h
  __m256i w0h = _mm256_srli_epi64(w0, 32);
  __m256i s1 = _mm256_add_epi64(w1, w0h);
  __m256i s1l = _mm256_and_si256(s1, lomask);
  __m256i s1h = _mm256_srli_epi64(s1, 32);
  __m256i s2 = _mm256_add_epi64(w2, s1l);
  __m256i s2h = _mm256_srli_epi64(s2, 32);
  __m256i hi1 = _mm256_add_epi64(w3, s1h);
  return _mm256_add_epi64(hi1, s2h);
}

#ifdef LATTICE_HAS_AVX512IFMA
template <>
inline __m256i _mm256_il_mulhi_epi<52>(__m256i x, __m256i y) {
  LATTICE_CHECK_BOUNDS(ExtractValues(x).data(), 4, MaximumValue(52));
  LATTICE_CHECK_BOUNDS(ExtractValues(y).data(), 4, MaximumValue(52));
  __m256i zero = _mm256_set1_epi64x(0);
  return _mm256_madd52hi_epu64(zero, x, y);
}
#endif

// Multiply packed unsigned BitShift-bit integers in each 64-bit element of x
// and y to form a 2*BitShift-bit intermediate result.
// Returns the high BitShift-bit unsigned integer from the intermediate result
template <int BitShift>
inline __m512i _mm512_il_mulhi_epi(__m512i x, __m512i y);

template <>
inline __m512i _mm512_il_mulhi_epi<64>(__m512i x, __m512i y) {
  // https://stackoverflow.com/questions/28807341/simd-signed-with-unsigned-multiplication-for-64-bit-64-bit-to-128-bit
  __m512i lomask = _mm512_set1_epi64(0x00000000ffffffff);
  __m512i xh =
      _mm512_shuffle_epi32(x, (_MM_PERM_ENUM)0xB1);  // x0l, x0h, x1l, x1h
  __m512i yh =
      _mm512_shuffle_epi32(y, (_MM_PERM_ENUM)0xB1);  // y0l, y0h, y1l, y1h
  __m512i w0 = _mm512_mul_epu32(x, y);               // x0l*y0l, x1l*y1l
  __m512i w1 = _mm512_mul_epu32(x, yh);              // x0l*y0h, x1l*y1h
  __m512i w2 = _mm512_mul_epu32(xh, y);              // x0h*y0l, x1h*y0l
  __m512i w3 = _mm512_mul_epu32(xh, yh);             // x0h*y0h, x1h*y1h
  __m512i w0h = _mm512_srli_epi64(w0, 32);
  __m512i s1 = _mm512_add_epi64(w1, w0h);
  __m512i s1l = _mm512_and_si512(s1, lomask);
  __m512i s1h = _mm512_srli_epi64(s1, 32);
  __m512i s2 = _mm512_add_epi64(w2, s1l);
  __m512i s2h = _mm512_srli_epi64(s2, 32);
  __m512i hi1 = _mm512_add_epi64(w3, s1h);
  return _mm512_add_epi64(hi1, s2h);
}

#ifdef LATTICE_HAS_AVX512IFMA
template <>
inline __m512i _mm512_il_mulhi_epi<52>(__m512i x, __m512i y) {
  LATTICE_CHECK_BOUNDS(ExtractValues(x).data(), 8, MaximumValue(52));
  LATTICE_CHECK_BOUNDS(ExtractValues(y).data(), 8, MaximumValue(52));
  __m512i zero = _mm512_set1_epi64(0);
  return _mm512_madd52hi_epu64(zero, x, y);
}
#endif

// Multiply packed unsigned BitShift-bit integers in each 64-bit element of x
// and y to form a 104-bit intermediate result.
// Returns the low BitShift-bit unsigned integer from the intermediate result
template <int BitShift>
inline __m512i _mm512_il_mullo_epi(__m512i x, __m512i y);

template <>
inline __m512i _mm512_il_mullo_epi<64>(__m512i x, __m512i y) {
  return _mm512_mullo_epi64(x, y);
}

#ifdef LATTICE_HAS_AVX512IFMA
template <>
inline __m512i _mm512_il_mullo_epi<52>(__m512i x, __m512i y) {
  LATTICE_CHECK_BOUNDS(ExtractValues(x).data(), 8, MaximumValue(52));
  LATTICE_CHECK_BOUNDS(ExtractValues(y).data(), 8, MaximumValue(52));
  __m512i zero = _mm512_set1_epi64(0);
  return _mm512_madd52lo_epu64(zero, x, y);
}
#endif

// Multiply packed unsigned BitShift-bit integers in each 64-bit element of y
// and z to form a 2*BitShift-bit intermediate result. The low BitShift bits of
// the result are added to x, then the result is returned.
template <int BitShift>
inline __m512i _mm512_il_mullo_add_epi(__m512i x, __m512i y, __m512i z);

#ifdef LATTICE_HAS_AVX512IFMA
template <>
inline __m512i _mm512_il_mullo_add_epi<52>(__m512i x, __m512i y, __m512i z) {
  LATTICE_CHECK_BOUNDS(ExtractValues(y).data(), 8, MaximumValue(52));
  LATTICE_CHECK_BOUNDS(ExtractValues(z).data(), 8, MaximumValue(52));
  return _mm512_madd52lo_epu64(x, y, z);
}
#endif

template <>
inline __m512i _mm512_il_mullo_add_epi<64>(__m512i x, __m512i y, __m512i z) {
  __m512i prod = _mm512_mullo_epi64(y, z);
  return _mm512_add_epi64(x, prod);
}

// Returns x mod p; assumes 0 < x < 2p
// x mod p == x >= p ? x - p : x
//         == min(x - p, x)
inline __m256i _mm256_il_small_mod_epu64(__m256i x, __m256i p) {
  return _mm256_min_epu64(x, _mm256_sub_epi64(x, p));
}

// Returns x mod p; assumes 0 < x < 2p
// x mod p == x >= p ? x - p : x
//         == min(x - p, x)
inline __m512i _mm512_il_small_mod_epu64(__m512i x, __m512i p) {
  return _mm512_min_epu64(x, _mm512_sub_epi64(x, p));
}

// Returns (x + y) mod p; assumes 0 < x, y < p
// x += y - p;
// if (x < 0) x+= p
// return x
inline __m512i _mm512_il_small_add_mod_epi64(__m512i x, __m512i y, __m512i p) {
  LATTICE_CHECK_BOUNDS(ExtractValues(x).data(), 8, ExtractValues(p)[0]);
  LATTICE_CHECK_BOUNDS(ExtractValues(y).data(), 8, ExtractValues(p)[0]);
  return _mm512_il_small_mod_epu64(_mm512_add_epi64(x, y), p);

  // __m512i v_diff = _mm512_sub_epi64(y, p);
  // x = _mm512_add_epi64(x, v_diff);
  // __mmask8 sign_bits = _mm512_movepi64_mask(x);
  // return _mm512_mask_add_epi64(x, sign_bits, x, p);
}

inline __mmask8 _mm512_il_cmp_epu64_mask(__m512i a, __m512i b, CMPINT cmp) {
  switch (cmp) {
    case CMPINT::EQ:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::EQ));
    case CMPINT::LT:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::LT));
    case CMPINT::LE:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::LE));
    case CMPINT::FALSE:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::FALSE));
    case CMPINT::NE:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::NE));
    case CMPINT::NLT:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::NLT));
    case CMPINT::NLE:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::NLE));
    case CMPINT::TRUE:
      return _mm512_cmp_epu64_mask(a, b, static_cast<int>(CMPINT::TRUE));
  }
  __mmask8 dummy = 0;  // Avoid end of non-void function warning
  return dummy;
}

// Returns c[i] = a[i] CMP b[i] ? match_value : 0
inline __m512i _mm512_il_cmp_epi64(__m512i a, __m512i b, CMPINT cmp,
                                   uint64_t match_value) {
  __mmask8 mask = _mm512_il_cmp_epu64_mask(a, b, cmp);
  return _mm512_maskz_broadcastq_epi64(mask, _mm_set1_epi64x(match_value));
}

// Returns c[i] = a[i] CMP b[i] ? match_value : 0
inline __m512i _mm512_il_cmp_epi64(__m512i a, __m512i b, int cmp,
                                   uint64_t match_value) {
  return _mm512_il_cmp_epi64(a, b, static_cast<CMPINT>(cmp), match_value);
}

// Returns c[i] = a[i] >= b[i] ? match_value : 0
inline __m512i _mm512_il_cmpge_epu64(__m512i a, __m512i b,
                                     uint64_t match_value) {
  return _mm512_il_cmp_epi64(a, b, CMPINT::NLT, match_value);
}

// Returns c[i] = a[i] < b[i] ? match_value : 0
inline __m512i _mm512_il_cmplt_epu64(__m512i a, __m512i b,
                                     uint64_t match_value) {
  return _mm512_il_cmp_epi64(a, b, CMPINT::LT, match_value);
}

// Returns c[i] = a[i] <= b[i] ? match_value : 0
inline __m512i _mm512_il_cmple_epu64(__m512i a, __m512i b,
                                     uint64_t match_value) {
  return _mm512_il_cmp_epi64(a, b, CMPINT::LE, match_value);
}

// Computes x + y mod 2^BitShift and stores the result in c.
// Returns the overflow bit
template <int BitShift>
inline __m512i _mm512_il_add_epu(__m512i x, __m512i y, __m512i* c);

template <>
inline __m512i _mm512_il_add_epu<64>(__m512i x, __m512i y, __m512i* c) {
  *c = _mm512_add_epi64(x, y);
  return _mm512_il_cmplt_epu64(*c, x, 1);
}

template <>
inline __m512i _mm512_il_add_epu<52>(__m512i x, __m512i y, __m512i* c) {
  __m512i vtwo_pow_52 = _mm512_set1_epi64(1UL << 52);
  __m512i sum = _mm512_add_epi64(x, y);
  __m512i carry = _mm512_il_cmpge_epu64(sum, vtwo_pow_52, 1);
  *c = _mm512_il_small_mod_epu64(sum, vtwo_pow_52);
  return carry;
}

// returns x mod p, computed via Barrett reduction
// @param p_barr floor(2^64 / p)
inline __m512i _mm512_il_barrett_reduce64(__m512i x, __m512i p,
                                          __m512i p_barr) {
  __m512i rnd1_hi = _mm512_il_mulhi_epi<64>(x, p_barr);

  // // Barrett subtraction
  // tmp[0] = input - tmp[1] * modulus.value();
  __m512i tmp1_times_mod = _mm512_il_mullo_epi<64>(rnd1_hi, p);
  x = _mm512_sub_epi64(x, tmp1_times_mod);
  // Correction
  x = _mm512_il_small_mod_epu64(x, p);
  return x;
}

// Concatenate packed 64-bit integers in x and y, producing an intermediate
// 128-bit result. Shift the result right by BitShift bits, and return the lower
// 64 bits.
template <int BitShift>
inline __m512i _mm512_il_shrdi_epi64(__m512i x, __m512i y) {
#ifdef LATTICE_HAS_AVX512IFMA
  return _mm512_shrdi_epi64(x, y, BitShift);
#else
  __m512i c_lo = _mm512_srli_epi64(x, BitShift);
  __m512i c_hi = _mm512_slli_epi64(y, 64 - (BitShift));
  return _mm512_add_epi64(c_lo, c_hi);
#endif
}

// Concatenate packed 64-bit integers in x and y, producing an intermediate
// 128-bit result. Shift the result right by bit_shift bits, and return the
// lower 64 bits. The bit_shift is a run-time argument, rather than a
// compile-time template parameter, so we can't use _mm512_shrdi_epi64
inline __m512i _mm512_il_shrdi_epi64(__m512i x, __m512i y, int bit_shift) {
  __m512i c_lo = _mm512_srli_epi64(x, bit_shift);
  __m512i c_hi = _mm512_slli_epi64(y, 64 - bit_shift);
  return _mm512_add_epi64(c_lo, c_hi);
}

}  // namespace lattice
}  // namespace intel
