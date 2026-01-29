//===- Common/Box.hpp -----------------------------------------------===//
//
// Copyright (C) 2024 Ninefold
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
/// This file defines the Box alias.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Support/Alloc.hpp>
#include <Common/D/CheckAlloc.hpp>

namespace exi {

template <typename T, typename D = std::default_delete<T>>
using Box = std::unique_ptr<T, D>;

template <typename T>
using MiBox = Box<T, GlobalDelete<T>>;

template <typename T>
inline MiBox<T> make_miunique(auto&&...Args) {
  return MiBox<T>(_new<T>(EXI_FWD(Args)...));
}

template <typename T>
struct DtorOnlyDeleter {
  inline void operator()(T* Ptr) const {
    std::destroy_at(Ptr);
  }
};

template <typename T>
struct DtorOnlyDeleter<T[]> {
  COMPILE_FAILURE(DtorOnlyDeleter,
                  "T[] is not supported!");
};

template <typename T>
using DtorOnlyBox = Box<T, DtorOnlyDeleter<T>>;

} // namespace exi
