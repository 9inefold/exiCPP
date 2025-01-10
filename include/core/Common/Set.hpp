//===- Common/Set.hpp -----------------------------------------------===//
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

#include <Support/Alloc.hpp>
#include <set>
#include <unordered_set>

namespace exi {

// TODO: Add custom hashing

template <
  class K,
  typename Hash = std::hash<K>,
  typename Pred = std::equal_to<K>>
using Set = std::unordered_set<K, Hash, Pred, Allocator<K>>;

template <
  class K,
  typename Pred = std::less<K>>
using OrderedSet = std::set<K, Pred, Allocator<K>>;

} // namespace exi
