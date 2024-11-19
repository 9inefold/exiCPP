//===- unit/Testing.hpp ---------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
//     limitations under the License.
//
//===----------------------------------------------------------------===//

#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <string_view>
#include <vector>

#ifdef EXICPP_FULL_TESTS
# undef  EXICPP_FULL_TESTS
# define EXICPP_FULL_TESTS 1
#else
# define EXICPP_FULL_TESTS 0
#endif

inline constexpr std::string_view test_dir = EXICPP_TEST_DIR;
inline constexpr std::string_view exificent_dir = EXICPP_EXIFICIENT_DIR;
inline constexpr std::string_view exificent = EXICPP_EXIFICIENT;

template <typename T, class A>
class ResizeAdaptor {
  using VecType = std::vector<T, A>;
public:
  ResizeAdaptor(VecType& V) :
   Vec(&V), OldSize(V.size()) {
  }

  ~ResizeAdaptor() {
    Vec->resize(this->OldSize);
  }

  VecType& get() const& {
    return *this->Vec;
  }
  
  VecType* operator->() const& {
    return this->Vec;
  }

private:
  VecType* Vec;
  std::size_t OldSize = 0;
};

template <typename T, class A>
ResizeAdaptor(std::vector<T, A>&) -> ResizeAdaptor<T, A>;

inline std::filesystem::path get_test_dir() {
  return std::filesystem::path(test_dir);
}
