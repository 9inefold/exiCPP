//===- Common/ManualRebind.hpp --------------------------------------===//
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
/// This file defines a wrapper which stops arbitrary reassignment.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Support/ErrorHandle.hpp>
#include <memory>
#include <type_traits>

namespace exi {

// TODO: Cast traits
template <typename From> struct simplify_type;

/// A wrapper used for marking certain objects as "non-assignable". This means
/// values won't accidentally be changed, but can still be explicitly updated.
/// It also helps avoid `const` member semantics.
template <typename T> class ManualRebind {
  using type = std::remove_const_t<T>;
  type Data {};

public:
  constexpr ManualRebind() = default;
  constexpr ManualRebind(const ManualRebind&) = default;
  constexpr ManualRebind(ManualRebind&&) = default;
  constexpr ManualRebind(auto&& First, auto&&...Rest)
   : Data(EXI_FWD(First), EXI_FWD(Rest)...) {}
  
  ManualRebind& operator=(const ManualRebind&) = delete;
  ManualRebind& operator=(ManualRebind&&) = delete;

  /// Assigns a new value to the object.
  EXI_INLINE constexpr type& assign(auto&& Val) {
    this->Data = EXI_FWD(Val);
    return Data;
  }

  /// Destroys the old value, and constructs a new one in its place.
  EXI_INLINE constexpr type&
   emplace(auto&&...Args) /* TODO: Add [[*::reinitializes]]? */ {
    if constexpr (!std::is_trivially_destructible_v<type>)
      std::destroy_at(std::addressof(Data));
    std::construct_at(std::addressof(Data), EXI_FWD(Args)...);
    return Data;
  }

  EXI_INLINE constexpr type* data() {
    return std::addressof(Data);
  }
  EXI_INLINE constexpr const type* data() const {
    return std::addressof(Data);
  }

  EXI_INLINE constexpr type& get() { return Data; }
  EXI_INLINE constexpr const type& get() const { return Data; }

  constexpr type* operator->() { return data(); }
  constexpr const type* operator->() const { return data(); }

  constexpr type& operator*() { return Data; }
  constexpr const type& operator*() const { return Data; }

  EXI_INLINE constexpr operator type&() { return Data; }
  EXI_INLINE constexpr operator const type&() const { return Data; }
};

/// Same basic concept as `ManualRebind`, but for the specific case of pointer
/// values. It allows you to use the indirection operator on the underlying
/// pointer (`T*`), instead of on the pointer itself (`T**`).
template <typename T> class ManualRebindPtr {
  using type = T*;
  T* Data = nullptr;
public:
  constexpr ManualRebindPtr() = default;
  constexpr ManualRebindPtr(const ManualRebindPtr&) = default;
  constexpr ManualRebindPtr(ManualRebindPtr&&) = default;
  constexpr ManualRebindPtr(T* Data) : Data(Data) {}
  
  ManualRebindPtr& operator=(const ManualRebindPtr&) = delete;
  ManualRebindPtr& operator=(ManualRebindPtr&&) = delete;

  /// Assigns a new value to the object.
  EXI_INLINE constexpr T* assign(T* Val) {
    this->Data = Val;
    return Data;
  }

  /// Same as assignment.
  EXI_INLINE constexpr T* emplace(T* Val) {
    return this->assign(Val);
  }

  EXI_INLINE constexpr T* data() const {
    return Data;
  }

  EXI_INLINE constexpr T& get() const {
    exi_invariant(Data != nullptr);
    return *Data;
  }

  EXI_INLINE constexpr T* operator->() const { return data(); }
  EXI_INLINE constexpr T& operator*() const { return get(); }
  EXI_INLINE constexpr operator T*() const { return Data; }
};

} // namespace exi
