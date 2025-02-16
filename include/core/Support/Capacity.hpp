//===- Support/Capacity.hpp -----------------------------------------===//
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
/// This file defines a utility function for calculating the memory usage
/// of a container in bytes. 
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <concepts>

namespace exi {
namespace H {
/// Trait for figuring out if a container has `.capacity_in_bytes()`.
template <class Container>
concept has_capacity_in_bytes
 = requires(const Container& Val) {
  { Val.capacity_in_bytes() } -> std::convertible_to<usize>;
};
} // namespace H

template <class Container>
EXI_INLINE static usize capacity_in_bytes(const Container& Val) {
  if constexpr (H::has_capacity_in_bytes<Container>) {
    return Val.capacity_in_bytes();
  } else {
    using ValT = typename Container::value_type;
    // This default definition of capacity should work for things like
    // std::vector and friends.  More specialized versions will work for others.
    return Val.capacity() * sizeof(ValT);
  }
}

} // namespace exi
