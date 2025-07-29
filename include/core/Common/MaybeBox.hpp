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
#include <Common/Ref.hpp>
#include <Common/PointerIntPair.hpp>
#include <type_traits>

// TODO: Add custom deleters?

namespace exi {
template <typename From> struct simplify_type;

namespace H {
template <typename T, bool IsPacked>
concept should_use_packed_repr
  = IsPacked && PointerLikeTypeTraits<T*>::NumLowBitsAvailable > 0;
} // namespace H

/// `MaybeBoxBase` for unpacked data.
template <typename T, bool IsPacked>
class MaybeBoxBase {
protected:
  /// Returns if the `MaybeBox` is packed.
  static constexpr bool is_packed = false;
  /// Pointer and int as two values.
  std::pair<T*, bool> Data;

  ALWAYS_INLINE void setData(T* Ptr, bool Owned) {
    Data.first = Ptr;
    Data.second = Owned;
  }
  ALWAYS_INLINE void clearData() {
    this->setData(nullptr, false);
  }
  ALWAYS_INLINE void deleteData() {
    if (owned())
      delete this->get();
  }

public:
  constexpr MaybeBoxBase() = default;
  constexpr MaybeBoxBase(T* Ptr, bool Owned) : Data(Ptr, Ptr ? Owned : false) {}
  constexpr MaybeBoxBase(std::nullptr_t) : MaybeBoxBase() {}
  ~MaybeBoxBase() { this->deleteData(); }

  /// Get the stored pointer.
  T* get() const { return Data.first; }
  /// Get the stored pointer.
  T* data() const { return Data.first; }
  /// Return if the pointer is owned or not.
  bool owned() const { return Data.second; }
  /// Return the pointer and owned status at the same time.
  std::pair<T*, bool> dataAndOwned() const { return Data; }
};

/// `MaybeBoxBase` for packed data.
template <typename T, bool IsPacked>
requires H::should_use_packed_repr<T, IsPacked>
class MaybeBoxBase<T, IsPacked> {
protected:
  /// Returns if the `MaybeBox` is packed.
  static constexpr bool is_packed = true;
  /// Pointer and int packed into one.
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
  constexpr MaybeBoxBase() = default;
  constexpr MaybeBoxBase(T* Ptr, bool Owned) : Data(Ptr, Ptr ? Owned : false) {}
  constexpr MaybeBoxBase(std::nullptr_t) : MaybeBoxBase() {}
  ~MaybeBoxBase() { this->deleteData(); }

  /// Get the stored pointer.
  T* get() const { return Data.getPointer(); }
  /// Get the stored pointer.
  T* data() const { return Data.getPointer(); }
  /// Return if the pointer is owned or not.
  bool owned() const { return Data.getInt(); }
  /// Return the pointer and owned status at the same time.
  std::pair<T*, bool> dataAndOwned() const {
    auto [Ptr, Owned] = Data;
    return {Ptr, Owned};
  }
};

/// This class is used when a pointer may or may not be owned.
/// If the input type is a `nullptr, `T&`, `Naked<T>`, or `Option<T&>`, it will
/// be marked as unowned. If the input is a `Box<T>`, it will be owned.
/// Otherwise ownedness is explicitly provided by the user with `(Ptr, Owned)`.
/// @tparam Packed If the pointer/int pair should be packed. Default is `false`.
template <typename T, bool Packed = false>
class MaybeBox : public MaybeBoxBase<T, Packed> {
  template <typename, bool> friend class MaybeBox;
  using BaseT = MaybeBoxBase<T, Packed>;
  using BaseT::setData;
  using BaseT::clearData;
  using BaseT::deleteData;
public:
  using BaseT::is_packed;
  using BaseT::BaseT;
  MaybeBox(const MaybeBox&) = delete;
  ~MaybeBox()
    EXI_WARNING_IF(Packed && !is_packed,
      "No bits were available for packing! "
      "Try adjusting object alignment.") = default;

  template <class U, bool UPacked>
  requires std::convertible_to<U*, T*>
  MaybeBox(MaybeBox<U, UPacked>&& O) : BaseT(O.get(), O.owned()) {
    O.clearData();
  }

  MaybeBox(Naked<T> Ptr) : BaseT(Ptr.get(), false) {}
  MaybeBox(T& Val EXI_LIFETIMEBOUND) : BaseT(&Val, false) {}
  MaybeBox(Ref<T> Val) : BaseT(&*Val, false) {}
  MaybeBox(Box<T>&& Ptr) : BaseT(Ptr.release(), true) {}
  MaybeBox(Option<T&> Opt) : BaseT(Opt ? &*Opt : nullptr, false) {}

  MaybeBox& operator=(const MaybeBox&) = delete;
  MaybeBox& operator=(MaybeBox&& O) {
    BaseT::deleteData();
    BaseT::Data = std::move(O.Data);
    O.BaseT::clearData();
    return *this;
  }

  MaybeBox& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  MaybeBox& operator=(Naked<T> Ptr) {
    BaseT::deleteData();
    BaseT::setData(Ptr.get(), false);
    return *this;
  }

  MaybeBox& operator=(T& Ref EXI_LIFETIMEBOUND) {
    BaseT::deleteData();
    BaseT::setData(&Ref, false);
    return *this;
  }

  MaybeBox& operator=(Box<T>&& Ptr) {
    BaseT::deleteData();
    BaseT::setData(Ptr.release(), true);
    return *this;
  }

  MaybeBox& operator=(Option<T&> Opt) {
    BaseT::deleteData();
    BaseT::setData(Opt.data(), false);
    return *this;
  }

  /// Set pointer with potentially owned data.
  void set(T* Ptr, bool Owned) {
    BaseT::deleteData();
    setData(Ptr, Ptr ? Owned : false);
  }

  /// Set pointer with unowned data.
  void set(T* Ptr) {
    BaseT::deleteData();
    setData(Ptr, false);
  }

  /// Reset container, null and unowned.
  void reset() {
    BaseT::deleteData();
    BaseT::clearData();
  }

  using BaseT::data;

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

/// A `MaybeBox` which is packed by default.
template <typename T>
using PackedMaybeBox = MaybeBox<T, /*Packed=*/true>;

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
template <typename T, bool IsPacked>
struct simplify_type<MaybeBox<T, IsPacked>> {
  using From = MaybeBox<T, IsPacked>;
  using SimpleType = T;
  static SimpleType& getSimplifiedValue(From& Val) { return *Val; }
};

} // namespace exi
