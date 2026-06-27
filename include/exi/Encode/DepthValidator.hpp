//===- exi/Encode/DepthValidator.hpp --------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file implements DepthValidator, which ensures the depth is the same at
/// the beginning and end of the scope..
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>
#if EXI_ASSERTS
# include <core/Support/Format.hpp>
#endif

namespace exi::encode {

#if EXI_INVARIANTS
template <class T> class DepthValidator {
  T& Data;
  usize OldDepth = 0;
  bool Active = false;
public:
  DepthValidator(T& Data) : Data(Data), OldDepth(Data.depth()) {}
  void activate() { this->Active = true; }
  EXI_NO_INLINE ~DepthValidator() {
    if EXI_UNLIKELY(Active && Data.depth() != OldDepth)
      exi::format_fatal_error(
        "Scope was not restored correctly! "
        "Expected {}, got {}.", OldDepth, Data.depth());
  }
};
#else
template <class T> class DepthValidator {
public:
  ALWAYS_INLINE constexpr DepthValidator(T&) {}
  ALWAYS_INLINE void activate() {}
};
#endif

template <typename T>
DepthValidator(T&) -> DepthValidator<T>;

} // namespace exi::encode
