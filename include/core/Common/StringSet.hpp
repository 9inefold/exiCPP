//===- Common/StringSet.hpp -----------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file defines StringSet, a set-like wrapper for StringMap.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/StringMap.hpp>

namespace exi {

/// StringSet - A wrapper for StringMap that provides set-like functionality.
template <class AllocatorT = MallocAllocator>
class StringSet : public StringMap<nullopt_t, AllocatorT> {
  using Base = StringMap<nullopt_t, AllocatorT>;

public:
  StringSet() = default;
  StringSet(std::initializer_list<StrRef> initializer) {
    for (StrRef str : initializer)
      insert(str);
  }
  template <typename Container> explicit StringSet(Container &&C) {
    for (auto &&Str : C)
      insert(Str);
  }
  explicit StringSet(AllocatorT a) : Base(a) {}

  std::pair<typename Base::iterator, bool> insert(StrRef key) {
    return Base::try_emplace(key);
  }

  template <typename InputIt>
  void insert(InputIt begin, InputIt end) {
    for (auto It = begin; It != end; ++It)
      insert(*It);
  }

  template <typename ValueTy>
  std::pair<typename Base::iterator, bool>
  insert(const StringMapEntry<ValueTy> &mapEntry) {
    return insert(mapEntry.getKey());
  }

  /// Check if the set contains the given \c key.
  bool contains(StrRef key) const { return Base::FindKey(key) != -1; }
};

} // namespace exi
