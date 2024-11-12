//===- utils/STL.hpp ------------------------------------------------===//
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

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mimalloc.h>

template <typename K, typename V>
using Map = std::unordered_map<K, V,
  std::hash<K>, std::equal_to<K>,
  mi_stl_allocator<std::pair<const K, V>>>;

template <typename K>
using Set = std::unordered_set<K,
  std::hash<K>, std::equal_to<K>,
  mi_stl_allocator<K>>;

template <typename T>
using Vec = std::vector<T, mi_stl_allocator<T>>;
