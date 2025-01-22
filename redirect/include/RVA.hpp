//===- RVA.hpp ------------------------------------------------------===//
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
/// This file implements a helper for dealing with
/// relative value addresses (RVAs).
///
//===----------------------------------------------------------------===//

#pragma once

#include <ArrayRef.hpp>
#include <Mem.hpp>

namespace re {

struct RVAHandler {
  template <typename T>
  RVAHandler(T* Data, usize Size) :
   Base(ptr_cast<byte>(Data)), Size(Size) {
    static_assert(sizeof_v<T> <= 1);
  }

public:
  template <typename T = void>
  T* get(usize Off) const {
    // Don't fuck up..
    re_assert((Off + sizeof_v<T>) < this->Size);
    return aligned_ptr_cast<T>(Base + Off);
  }

  template <typename T>
  MutArrayRef<T> getArr(usize Off, usize Count) const {
    static_assert(sizeof_v<T> != 0, "Cannot use void!");
    re_assert((Off + Count * sizeof_v<T>) < this->Size);
    return MutArrayRef<T>(get<T>(), Count);
  }

private:
  byte* Base = nullptr;
  usize Size = 0;
};

} // namespace re
