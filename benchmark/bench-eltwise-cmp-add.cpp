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

#include <benchmark/benchmark.h>

#include <random>
#include <vector>

#include "eltwise/eltwise-cmp-add-internal.hpp"
#include "eltwise/eltwise-cmp-add.hpp"
#include "logging/logging.hpp"

#ifdef LATTICE_HAS_AVX512DQ
#include "eltwise/eltwise-cmp-add-avx512.hpp"
#endif

namespace intel {
namespace lattice {

//=================================================================

// state[0] is the degree
static void BM_EltwiseCmpAddNative(benchmark::State& state) {  //  NOLINT
  size_t input_size = state.range(0);

  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(0, 100);

  uint64_t bound = distrib(gen);
  uint64_t diff = distrib(gen);
  std::vector<uint64_t> input1(input_size);
  for (size_t i = 0; i < input_size; ++i) {
    input1[i] = distrib(gen);
  }

  for (auto _ : state) {
    EltwiseCmpAddNative(input1.data(), CMPINT::NLT, bound, diff, input_size);
  }
}

BENCHMARK(BM_EltwiseCmpAddNative)
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(3.0)
    ->Args({1024})
    ->Args({4096})
    ->Args({16384});

//=================================================================

#ifdef LATTICE_HAS_AVX512DQ
// state[0] is the degree
static void BM_EltwiseCmpAddAVX512(benchmark::State& state) {  //  NOLINT
  size_t input_size = state.range(0);

  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(0, 100);

  uint64_t bound = 50;
  uint64_t diff = distrib(gen);
  std::vector<uint64_t> input1(input_size);
  for (size_t i = 0; i < input_size; ++i) {
    input1[i] = distrib(gen);
  }

  for (auto _ : state) {
    EltwiseCmpAddAVX512(input1.data(), CMPINT::NLT, bound, diff, input_size);
  }
}

BENCHMARK(BM_EltwiseCmpAddAVX512)
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(3.0)
    ->Args({1024})
    ->Args({4096})
    ->Args({16384});
#endif

}  // namespace lattice
}  // namespace intel