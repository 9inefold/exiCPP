//===- Common/MaybeBox.hpp ------------------------------------------===//
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
/// This file provides an interface for pointers which may be boxed.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Box.hpp>
#include <Common/Naked.hpp>
#include <Common/Option.hpp>
#include <Common/PointerIntPair.hpp>
#include <type_traits>

namespace exi {
template <typename From> struct simplify_type;

/// This class is used when a pointer may or may not be owned.
/// If the input type is a `nullptr, `T&`, `Naked<T>`, or `Option<T&>`, it will
/// be marked as unowned. If the input is a `Box<T>`, it will be owned.
/// Otherwise ownedness is explicitly provided by the user with `(Ptr, Owned)`.
template <typename T> class MaybeBox {
  static_assert(PointerLikeTypeTraits<T*>::NumLowBitsAvailable > 0);
  PointerIntPair<T*, 1, bool> Data;

  ALWAYS_INLINE void setData(T* Ptr, bool Owned) {
    Data.setPointerAndInt(Ptr, Owned);
  }
  ALWAYS_INLINE void deleteData() {
    if (owned())
      delete this->get();
  }
  ALWAYS_INLINE void clearData() {
    this->setData(nullptr, false);
  }

public:
  constexpr MaybeBox() = default;
  MaybeBox(const MaybeBox&) = delete;
  MaybeBox(MaybeBox&& O) : Data(std::move(O.Data)) { O.clearData(); }

  constexpr MaybeBox(std::nullptr_t) : MaybeBox() {}
  MaybeBox(T* Ptr, bool Owned) : Data(Ptr, Ptr ? Owned : false) {}
  MaybeBox(Naked<T> Ptr) : Data(Ptr.get(), false) {}
  MaybeBox(T& Ref EXI_LIFETIMEBOUND) : Data(&Ref, false) {}
  MaybeBox(Box<T>&& Ptr) : Data(Ptr.release(), true) {}
  MaybeBox(Option<T&> Opt) : Data(Opt ? &*Opt : nullptr, false) {}

  ~MaybeBox() { this->deleteData(); }

  MaybeBox& operator=(const MaybeBox&) = delete;
  MaybeBox& operator=(MaybeBox&& O) {
    this->deleteData();
    this->Data = std::move(O.Data);
    O.clearData();
    return *this;
  }

  MaybeBox& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  MaybeBox& operator=(Naked<T> Ptr) {
    this->deleteData();
    this->setData(Ptr.get(), false);
    return *this;
  }

  MaybeBox& operator=(T& Ref EXI_LIFETIMEBOUND) {
    this->deleteData();
    this->setData(&Ref, false);
    return *this;
  }

  MaybeBox& operator=(Box<T>&& Ptr) {
    this->deleteData();
    this->setData(Ptr.release(), true);
    return *this;
  }

  MaybeBox& operator=(Option<T&> Opt) {
    this->deleteData();
    this->setData(Opt.data(), false);
    return *this;
  }

  /// Set pointer with potentially owned data.
  void set(T* Ptr, bool Owned) {
    this->deleteData();
    setData(Ptr, Ptr ? Owned : false);
  }

  /// Set pointer with unowned data.
  void set(T* Ptr) {
    this->deleteData();
    setData(Ptr, false);
  }

  /// Reset container, null and unowned.
  void reset() {
    this->deleteData();
    this->clearData();
  }

  /// Get the stored pointer.
  T* get() const { return Data.getPointer(); }
  /// Get the stored pointer.
  T* data() const { return Data.getPointer(); }
  /// Return if the pointer is owned or not.
  bool owned() const { return Data.getInt(); }

  /// Return the pointer and owned status at the same time.
  std::pair<T*, bool> dataAndOwned() {
    auto [Ptr, Owned] = Data;
    return {Ptr, Owned};
  }

  T* operator->() const {
    exi_assert(data(), "value is inactive!");
    return data();
  }
  T& operator*() const {
    exi_assert(data(), "value is inactive!");
    return *data();
  }

  explicit operator bool() const {
    return !!data();
  }
};

/// Deduction guide for a `MaybeBox` from a reference.
template <typename T> MaybeBox(T&) -> MaybeBox<std::remove_const_t<T>>;

/// Deduction guide for a `MaybeBox` from `(T*, bool)`.
template <typename T> MaybeBox(T*, bool) -> MaybeBox<T>;

/// Deduction guide for a `MaybeBox` from a `Naked`.
template <typename T> MaybeBox(Naked<T>) -> MaybeBox<T>;

/// Deduction guide for a `MaybeBox` from an `Option<_&>`.
template <typename T> MaybeBox(Option<T&>) -> MaybeBox<T>;

/// Deduction guide for a `MaybeBox` from a `Box`.
template <typename T> MaybeBox(Box<T>&&) -> MaybeBox<T>;

//////////////////////////////////////////////////////////////////////////
// TODO: Cast Traits

/// Provide a simplify_type specialized for MaybeBox.
template <typename T> struct simplify_type<MaybeBox<T>> {
  using From = MaybeBox<T>;
  using SimpleType = T;
  static SimpleType& getSimplifiedValue(From& Val) { return *Val; }
};

} // namespace exi
