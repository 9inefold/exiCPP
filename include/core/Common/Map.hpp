//===- Common/Map.hpp -----------------------------------------------===//
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
#include <map>
#include <unordered_map>

namespace exi {

// TODO: Add custom hashing

template <
  class K, class V,
  typename Hash = std::hash<K>,
  typename Pred = std::equal_to<K>>
using Map = std::unordered_map<
  K, V, Hash, Pred,
  Allocator<std::pair<const K, V>>>;

template <
  class K, class V,
  typename Pred = std::less<K>>
using OrderedMap = std::map<
  K, V, Pred,
  Allocator<std::pair<const K, V>>>;

} // namespace exi
