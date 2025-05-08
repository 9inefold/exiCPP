//===- Common/ManualDrop.hpp ----------------------------------------===//
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
/// This file defines a wrapper which requires the user to manually destroy it.
/// It is useful for statics which do not need to be destroyed.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Support/AlignedUnion.hpp>
#include <Support/ErrorHandle.hpp>
#include <new>

namespace exi {

/// Wrapper that allows the user to manually destroy the object.
template <typename T, bool Lazy = false>
class ManualDrop {
  using Ty = std::remove_const_t<T>;
  AlignedCharArrayUnion<Ty> Data;
#if EXI_ASSERTS
  bool Initialized = false;
#endif

  Ty* pointer() const {
#if EXI_ASSERTS
    exi_assert(Initialized, "Object has already been destroyed!");
#endif
    auto* Raw = reinterpret_cast<const Ty*>(Data.buffer);
    return std::launder(const_cast<Ty*>(Raw));
  }

public:
  ManualDrop(auto&&...Args) {
    new (&Data) Ty(EXI_FWD(Args)...);
#if EXI_ASSERTS
    Initialized = true;
#endif
  }

  void dtor() noexcept {
#if EXI_ASSERTS
    exi_assert(Initialized, "Object has already been destroyed!");
#endif
    pointer()->~T();
  }

  T* data() { return pointer(); }
  const T* data() const { return pointer(); }

  T& get() { return *pointer(); }
  const T& get() const { return *pointer(); }

  T& value() { return *pointer(); }
  const T& value() const { return *pointer(); }

  T* operator->() { return pointer(); }
  const T* operator->() const { return pointer(); }

  T& operator*() { return *pointer(); }
  const T& operator*() const { return *pointer(); }

  operator T&() { return *pointer(); }
  operator const T&() const { return *pointer(); }
};

} // namespace exi
