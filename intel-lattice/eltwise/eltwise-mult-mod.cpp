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

#include "eltwise/eltwise-mult-mod.hpp"

#include "eltwise/eltwise-mult-mod-internal.hpp"
#include "logging/logging.hpp"
#include "number-theory/number-theory.hpp"
#include "util/check.hpp"
#include "util/cpu-features.hpp"

#ifdef LATTICE_HAS_AVX512DQ
#include "eltwise/eltwise-mult-mod-avx512.hpp"
#endif

namespace intel {
namespace lattice {

// Algorithm 1 of https://hal.archives-ouvertes.fr/hal-01215845/document
void EltwiseMultModNative(uint64_t* operand1, const uint64_t* operand2,
                          const uint64_t n, const uint64_t modulus) {
  LATTICE_CHECK_BOUNDS(operand1, n, modulus);
  LATTICE_CHECK_BOUNDS(operand2, n, modulus);
  LATTICE_CHECK(modulus != 0, "Require modulus != 0");

  uint64_t logmod = std::log2l(modulus);

  // modulus < 2**N
  uint64_t N = logmod + 1;
  uint64_t D = N + N;
  uint64_t L = D;
  uint64_t barr_lo = (uint128_t(1) << L) / modulus;

#pragma GCC unroll 4
#pragma clang loop unroll_count(4)
  for (size_t i = 0; i < n; ++i) {
    uint64_t prod_hi, prod_lo, c2_hi, c2_lo, result;

    // Multiply inputs
    MultiplyUInt64(*operand1, *operand2, &prod_hi, &prod_lo);
    // C1 = D >> (N-1)

    uint64_t c1 = (prod_lo >> (N - 1)) + (prod_hi << (64 - (N - 1)));

    // C2 = C1 * barr_lo
    MultiplyUInt64(c1, barr_lo, &c2_hi, &c2_lo);

    // C3 = C2 >> (L - N + 1)
    uint64_t c3 = (c2_lo >> (L - N + 1)) + (c2_hi << (64 - (L - N + 1)));

    // C4 = prod_lo - (p * c3)_lo
    result = prod_lo - c3 * modulus;

    // Conditional subtraction
    *operand1 = result - (modulus & static_cast<uint64_t>(-static_cast<int64_t>(
                                        result >= modulus)));

    ++operand1;
    ++operand2;
  }
}

void EltwiseMultMod(uint64_t* operand1, const uint64_t* operand2,
                    const uint64_t n, const uint64_t modulus) {
#ifdef LATTICE_HAS_AVX512DQ
  if (has_avx512_dq) {
    if (modulus < (1UL << 50)) {
      EltwiseMultModAVX512Float(operand1, operand2, n, modulus);
      return;
    } else {
      EltwiseMultModAVX512Int(operand1, operand2, n, modulus);
      return;
    }
  }
#endif

  IVLOG(3, "Calling EltwiseMultModNative");
  EltwiseMultModNative(operand1, operand2, n, modulus);
}

void EltwiseMultModOofP(uint64_t* result, uint64_t* operand1,
                        const uint64_t* operand2, const uint64_t n,
                        const uint64_t modulus) {
#ifdef LATTICE_HAS_AVX512DQ
  if (has_avx512_dq) {
    if (modulus < (1UL << 50)) {
      EltwiseMultModAVX512FloatOofP(result, operand1, operand2, n, modulus);
      result = operand1;
      return;
    } else {
      EltwiseMultModAVX512IntOofP(result, operand1, operand2, n, modulus);
      result = operand1;
      return;
    }
  }
#endif

  IVLOG(3, "Calling EltwiseMultModNative");
  EltwiseMultModNative(operand1, operand2, n, modulus);
  for (int i = 0; i < static_cast<int>(n); i++) {
    result[i] = operand1[i];
  }
}
}  // namespace lattice
}  // namespace intel
