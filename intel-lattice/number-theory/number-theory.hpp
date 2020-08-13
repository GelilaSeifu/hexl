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

#include <limits>
#include <vector>

#include "logging/logging.hpp"
#include "util/check.hpp"

namespace intel {
namespace lattice {

// Computes floor(2^128 / modulus)
class Barrett128Factor {
 public:
  Barrett128Factor() = delete;

  explicit Barrett128Factor(uint64_t modulus) {
    constexpr uint128_t two_pow_64 = uint128_t(1) << 64;
    constexpr uint128_t two_pow_128_minus_1 = uint128_t(-1);

    // The Barrett factor is actually floor(2^128 / modulus)
    // But since modulus should be prime, modulus does not divide 2^128,
    // hence floor(2^128/modulus) = floor((2^128 - 1) / modulus)
    uint128_t barrett_factor = (two_pow_128_minus_1 / modulus);
    m_barrett_hi = barrett_factor >> 64;
    m_barrett_lo = barrett_factor % two_pow_64;
  }

  uint64_t Hi() const { return m_barrett_hi; }
  uint64_t Lo() const { return m_barrett_lo; }

 private:
  uint64_t m_barrett_hi;
  uint64_t m_barrett_lo;
};

// Stores an integer on which modular multiplication can be performed more
// efficicently, at the cost of some precomputation.
class MultiplyFactor {
 public:
  MultiplyFactor() = default;

  // Computes and stores the Barrett factor (operand << bit_shift) / modulus
  MultiplyFactor(uint64_t operand, uint64_t bit_shift, uint64_t modulus)
      : m_operand(operand) {
    LATTICE_CHECK(
        operand <= modulus,
        "operand " << operand << " must be less than modulus " << modulus);
    m_barrett_factor =
        static_cast<uint64_t>((uint128_t(operand) << bit_shift) / modulus);
  }

  inline uint64_t BarrettFactor() const { return m_barrett_factor; }
  inline uint64_t Operand() const { return m_operand; }

 private:
  uint64_t m_operand;
  uint64_t m_barrett_factor;
};

// Returns whether or not num is a power of two
inline bool IsPowerOfTwo(uint64_t num) { return num && !(num & (num - 1)); }

// Returns log2(x) for x a power of 2
inline uint64_t Log2(uint64_t x) {
  LATTICE_CHECK(IsPowerOfTwo(x), x << " not a power of 2");
  uint64_t ret = 0;
  while (x >>= 1) ++ret;
  return ret;
}

// Returns the maximum value that can be represented using bits bits
inline uint64_t MaximumValue(uint64_t bits) {
  LATTICE_CHECK(bits <= 64, "MaximumValue requires bits <= 64; got " << bits);
  if (bits == 64) {
    return std::numeric_limits<uint64_t>::max();
  }
  return (1UL << bits) - 1;
}

// Reverses the bits
uint64_t ReverseBitsUInt(uint64_t x, uint64_t bits);

// Returns a^{-1} mod modulus
uint64_t InverseUIntMod(uint64_t a, uint64_t modulus);

// Return x * y as 128-bit integer
inline uint128_t MultiplyUInt64(uint64_t x, uint64_t y) {
  return static_cast<uint128_t>(x) * y;
}

// Performs bit-wise division with 128b numerator and 64b denominator
// @param x Numerator in 2 64bit to represent 128b unsigned integer where
// x[1]=high 64b, x[0]=low 64b, returns n mod denominator after division
// @param y Denominator
inline uint64_t* DivideUInt128UInt64(uint64_t* x, uint64_t y) {
  uint64_t* result = new uint64_t[2];
  uint128_t n, q;

  n = (static_cast<uint128_t>(x[1]) << 64) | (static_cast<uint128_t>(x[0]));
  q = n / y;
  n -= q * y;

  x[0] = static_cast<uint64_t>(n);
  x[1] = 0;

  result[0] = static_cast<uint64_t>(q);        // low 64 bit
  result[1] = static_cast<uint64_t>(q >> 64);  // high 64 bit

  return result;
}

// Returns low 64bit of 128b/64b where x1=high 64b, x0=low 64b
inline uint64_t DivideUInt128UInt64Lo(uint64_t x0, uint64_t x1, uint64_t y) {
  uint128_t n, q;

  n = (static_cast<uint128_t>(x1) << 64) | (static_cast<uint128_t>(x0));
  q = n / y;

  return static_cast<uint64_t>(q);
}

// Returns high 64bit of 128b/64b where x1=high 64b, x0=low 64b
inline uint64_t DivideUInt128UInt64Hi(uint64_t x0, uint64_t x1, uint64_t y) {
  uint128_t n, q;

  n = (static_cast<uint128_t>(x1) << 64) | (static_cast<uint128_t>(x0));
  q = n / y;

  return static_cast<uint64_t>(q >> 64);
}

// Multiplies x * y as 128-bit integer.
// @param prod_hi Stores high 64 bits of product
// @param prod_lo Stores low 64 bits of product
inline void MultiplyUInt64(uint64_t x, uint64_t y, uint64_t* prod_hi,
                           uint64_t* prod_lo) {
  uint128_t prod = MultiplyUInt64(x, y);
  *prod_hi = static_cast<uint64_t>(prod >> 64);
  *prod_lo = static_cast<uint64_t>(prod);
}

// Return the high 128 minus BitShift bits of the 128-bit product x * y
template <int BitShift>
inline uint64_t MultiplyUInt64Hi(uint64_t x, uint64_t y) {
  uint128_t product = static_cast<uint128_t>(x) * y;
  return (uint64_t)(product >> BitShift);
}

// Returns (x * y) mod modulus
// Assumes x, y < modulus
uint64_t MultiplyUIntMod(uint64_t x, uint64_t y, uint64_t modulus);

// Returns (x + y) mod modulus
// Assumes x, y < modulus
uint64_t AddUIntMod(uint64_t x, uint64_t y, uint64_t modulus);

// Returns (x - y) mod modulus
// Assumes x, y < modulus
uint64_t SubUIntMod(uint64_t x, uint64_t y, uint64_t modulus);

// Returns base^exp mod modulus
uint64_t PowMod(uint64_t base, uint64_t exp, uint64_t modulus);

// Returns true whether root is a degree-th root of unity
// degree must be a power of two.
bool IsPrimitiveRoot(uint64_t root, uint64_t degree, uint64_t modulus);

// Tries to return a primtiive degree-th root of unity
// Returns -1 if no root is found
uint64_t GeneratePrimitiveRoot(uint64_t degree, uint64_t modulus);

// Returns true whether root is a degree-th root of unity
// degree must be a power of two.
uint64_t MinimalPrimitiveRoot(uint64_t degree, uint64_t modulus);

// Computes (x * y) mod modulus, except that the output is in [0, 2 * modulus]
// @param modulus_precon Pre-computed Barrett reduction factor
template <int BitShift>
inline uint64_t MultiplyUIntModLazy(uint64_t x, uint64_t y_operand,
                                    uint64_t y_barrett_factor, uint64_t mod) {
  LATTICE_CHECK(y_operand <= mod, "y_operand " << y_operand
                                               << " must be less than modulus "
                                               << mod);
  LATTICE_CHECK(mod <= MaximumValue(BitShift), "Modulus "
                                                   << mod << " exceeds bound "
                                                   << MaximumValue(BitShift));
  LATTICE_CHECK(x <= MaximumValue(BitShift),
                "Operand " << x << " exceeds bound " << MaximumValue(BitShift));

  uint64_t Q = MultiplyUInt64Hi<BitShift>(x, y_barrett_factor);
  return y_operand * x - Q * mod;
}

// Computes (x * y) mod modulus, except that the output is in [0, 2 * modulus]
template <int BitShift>
inline uint64_t MultiplyUIntModLazy(uint64_t x, uint64_t y, uint64_t modulus) {
  uint64_t y_barrett = (uint128_t(y) << BitShift) / modulus;
  return MultiplyUIntModLazy<BitShift>(x, y, y_barrett, modulus);
}

// Adds two unsigned 64-bit integers
// @param operand1 Number to add
// @param operand2 Number to add
// @param result Stores the sum
// @return The carry bit
inline unsigned char AddUInt64(uint64_t operand1, uint64_t operand2,
                               uint64_t* result) {
  *result = operand1 + operand2;
  return static_cast<unsigned char>(*result < operand1);
}

// Returns whether or not the input is prime
bool IsPrime(uint64_t n);

// Generates a list of num_primes primes in the range [2^(bit_size,
// 2^(bit_size+1)]. Ensures each prime p satisfies
// p % (2*ntt_size+1)) == 1
// @param num_primes Number of primes to generate
// @param bit_size Bit size of each prime
// @param ntt_size N such that each prime p satisfies p % (2N) == 1. N must be
// a power of two
std::vector<uint64_t> GeneratePrimes(size_t num_primes, size_t bit_size,
                                     size_t ntt_size = 1);

}  // namespace lattice
}  // namespace intel
