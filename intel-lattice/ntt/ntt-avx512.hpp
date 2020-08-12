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

#include <iostream>
#include <vector>

#include "logging/logging.hpp"
#include "ntt/ntt.hpp"
#include "number-theory/number-theory.hpp"
#include "util/avx512_util.hpp"

namespace intel {
namespace lattice {

template <int BitShift>
void NTT::ForwardTransformToBitReverseAVX512(
    const IntType degree, const IntType mod,
    const IntType* root_of_unity_powers,
    const IntType* precon_root_of_unity_powers, IntType* elements) {
  uint64_t twice_mod = mod << 1;

  __m512i v_modulus = _mm512_set1_epi64(mod);
  __m512i v_twice_mod = _mm512_set1_epi64(twice_mod);

  IVLOG(5, "root_of_unity_powers " << std::vector<uint64_t>(
               root_of_unity_powers, root_of_unity_powers + degree))
  IVLOG(5,
        "precon_root_of_unity_powers " << std::vector<uint64_t>(
            precon_root_of_unity_powers, precon_root_of_unity_powers + degree));

  IVLOG(5, "elements " << std::vector<uint64_t>(elements, elements + degree));

  size_t n = degree;

  CheckArguments(degree, mod);
  LATTICE_CHECK(
      CheckBounds(precon_root_of_unity_powers, n, MaximumValue(BitShift)),
      "precon_root_of_unity_powers too large");
  LATTICE_CHECK(CheckBounds(elements, n, MaximumValue(BitShift)),
                "elements too large");

  size_t t = (n >> 1);

  uint64_t* input = elements;

  for (size_t m = 1; m < n; m <<= 1) {
    size_t j1 = 0;
    for (size_t i = 0; i < m; i++) {
      size_t j2 = j1 + t;
      const uint64_t W_op = root_of_unity_powers[m + i];
      const uint64_t W_precon = precon_root_of_unity_powers[m + i];

      uint64_t* X = input + j1;
      uint64_t* Y = X + t;
      uint64_t tx;
      uint64_t Q;

      if (j2 - j1 < 8) {
#pragma GCC unroll 4
#pragma clang loop unroll_count(4)
        for (size_t j = j1; j < j2; j++) {
          // The Harvey butterfly: assume X, Y in [0, 4p), and return X', Y' in
          // [0, 4p).
          // See Algorithm 4 of https://arxiv.org/pdf/1205.2926.pdf
          // X', Y' = X + WY, X - WY (mod p).
          tx = *X - (twice_mod & static_cast<uint64_t>(
                                     -static_cast<int64_t>(*X >= twice_mod)));
          Q = MultiplyUIntModLazy<BitShift>(*Y, W_op, W_precon, mod);

          LATTICE_CHECK(tx + Q <= MaximumValue(BitShift),
                        "tx " << tx << " + Q " << Q << " excceds");
          LATTICE_CHECK(tx + twice_mod - Q <= MaximumValue(BitShift),
                        "tx " << tx << " + twice_mod " << twice_mod << " + Q "
                              << Q << " excceds");
          *X++ = tx + Q;
          *Y++ = tx + twice_mod - Q;
        }
      } else {
        __m512i v_W_operand = _mm512_set1_epi64(W_op);
        __m512i v_W_barrett = _mm512_set1_epi64(W_precon);

        __m512i* v_X_pt = reinterpret_cast<__m512i*>(X);
        __m512i* v_Y_pt = reinterpret_cast<__m512i*>(Y);

        for (size_t j = j1; j < j2; j += 8) {
          __m512i v_X = _mm512_loadu_si512(v_X_pt);
          __m512i v_Y = _mm512_loadu_si512(v_Y_pt);

          // tx = X >= twice_mod ? X - twice_mod : X
          __m512i v_tx = avx512_mod_epu64(v_X, v_twice_mod);

          // multiply_uint64_hw64(Wprime, *Y, &Q);
          __m512i v_Q = avx512_multiply_uint64_hi<BitShift>(v_W_barrett, v_Y);

          // Q = *Y * W - Q * modulus;
          // Use 64-bit multiply low, even when BitShift != 52
          __m512i tmp1 = avx512_multiply_uint64_lo<64>(v_Y, v_W_operand);
          __m512i tmp2 = avx512_multiply_uint64_lo<64>(v_Q, v_modulus);
          v_Q = _mm512_sub_epi64(tmp1, tmp2);

          // *X++ = tx + Q;
          v_X = _mm512_add_epi64(v_tx, v_Q);

          // *Y++ = tx + (two_times_modulus - Q);
          __m512i sub = _mm512_sub_epi64(v_twice_mod, v_Q);
          v_Y = _mm512_add_epi64(v_tx, sub);

          _mm512_storeu_si512(v_X_pt, v_X);
          _mm512_storeu_si512(v_Y_pt, v_Y);

          LATTICE_CHECK(CheckBounds(v_X, MaximumValue(BitShift)), "");
          LATTICE_CHECK(CheckBounds(v_Y, MaximumValue(BitShift)), "");

          ++v_X_pt;
          ++v_Y_pt;
        }
      }
      j1 += (t << 1);
    }
    t >>= 1;
  }

  if (n < 8) {
    for (size_t i = 0; i < n; ++i) {
      if (input[i] >= twice_mod) {
        input[i] -= twice_mod;
      }
      if (input[i] >= mod) {
        input[i] -= mod;
      }
    }
  } else {
    // n power of two at least 8 => n divisible by 8
    LATTICE_CHECK(n % 8 == 0, "degree " << degree << " not a power of 2");
    __m512i* v_X_pt = reinterpret_cast<__m512i*>(elements);
    for (size_t i = 0; i < n; i += 8) {
      __m512i v_X = _mm512_loadu_si512(v_X_pt);

      v_X = avx512_mod_epu64(v_X, v_twice_mod);
      v_X = avx512_mod_epu64(v_X, v_modulus);

      _mm512_storeu_si512(v_X_pt, v_X);

      ++v_X_pt;
    }
  }
}

}  // namespace lattice
}  // namespace intel