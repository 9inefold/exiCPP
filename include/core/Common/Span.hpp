//===- Common/Span.hpp ----------------------------------------------===//
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

#include "Fundamental.hpp"
#include <span>

namespace exi {

/// @brief Alias for `std::dynamic_extent`.
EXI_CONST usize dyn_extent = std::dynamic_extent;

/// @brief Same as `std::span<T, E>`.
template <typename T, usize Extent = dyn_extent>
using Span = std::span<T, Extent>;

/// @brief Maps `[T, E] -> [const T, E]`.
template <typename T, usize Extent = dyn_extent>
using ImmSpan = Span<const T, Extent>;

} // namespace exi
