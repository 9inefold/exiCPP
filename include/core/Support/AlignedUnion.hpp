//===- Support/AlignedUnion.hpp -------------------------------------===//
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
/// This file defines the AlignedCharArrayUnion class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Config.inc>
#include <type_traits>

namespace exi {

/// A suitably aligned and sized character array member which can hold elements
/// of any type.
///
/// This template is equivalent to std::aligned_union_t<1, ...>, but we cannot
/// use it due to a bug in the MSVC x86 compiler:
/// https://github.com/microsoft/STL/issues/1533
/// Using `alignas` here works around the bug.
template <typename T, typename...TT> struct AlignedCharArrayUnion {
  using AlignedUnion = std::aligned_union_t<1, T, TT...>;
  alignas(alignof(AlignedUnion)) char buffer[sizeof(AlignedUnion)];
};

} // namespace exi
