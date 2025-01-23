//===- OptInt.hpp ---------------------------------------------------===//
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
///
/// \file
/// This file defines an optional integer type.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>

namespace re {

struct nullopt_t {
  enum class _iInternal { E };
  using enum _iInternal;
  constexpr nullopt_t(_iInternal) {}
};

inline constexpr nullopt_t nullopt { nullopt_t::E };

//////////////////////////////////////////////////////////////////////////

template <typename Int> struct OptInt {
  static_assert(std::is_integral_v<Int>);
public:
  static constexpr Int Invalid() {
    if (std::is_unsigned_v<Int>) {
      return ~Int(0u);
    } else {
      using UInt = std::make_unsigned_t<Int>;
      constexpr UInt Val = 1 << (bitsizeof_v<Int> - 1);
      return Int(Val);
    }
  }

  constexpr OptInt(nullopt_t) : Data(Invalid()) {}
  constexpr OptInt(Int I = 0) : Data(I) {}

  bool isOk() const { return this->Data != Invalid(); }
  bool isErr() const { return !isOk(); }
  explicit operator bool() const { return isOk(); }

  Int ok() const {
    re_assert(isOk(), "Use of invalid value!");
    return Data;
  }

  Int operator*() const { return this->ok(); }
  explicit operator Int() const { return this->ok(); }

public:
  Int Data = Invalid();
};

template <typename Int>
OptInt(Int) -> OptInt<Int>;

} // namespace re
