//===- Common/Any.hpp -----------------------------------------------===//
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
///  This file provides Any, a non-template class modeled in the spirit of
///  std::any. The idea is to provide a type-safe replacement for C's void*.
///  It can hold a value of any copy-constructible copy-assignable type.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Box.hpp>
#include <Common/Features.hpp>
#include <Support/ErrorHandle.hpp>
#include <concepts>

namespace exi {

class Any;

namespace any_detail {

template <typename T>
concept enable_impl
	= !std::same_as<T, Any>
	// We also disable this overload when an `Any` object can be
	// converted to the parameter type because in that case,
	// this constructor may combine with that conversion during
	// overload resolution for determining copy
	// constructibility, and then when we try to determine copy
	// constructibility below we may infinitely recurse. This is
	// being evaluated by the standards committee as a potential
	// DR in `std::any` as well, but we're going ahead and
	// adopting it to work-around usage of `Any` with types that
	// need to be implicitly convertible from an `Any`.
	&& !std::is_convertible_v<Any, T>
	&& std::is_copy_constructible_v<T>;

template <typename MaybeRef>
concept enable = enable_impl<std::decay_t<MaybeRef>>;

} // namespace any_detail

class Any {
  // The `Typeid<T>::Id` static data member below is a globally unique
  // identifier for the type `T`. It is explicitly marked with default
  // visibility so that when `-fvisibility=hidden` is used, the loader still
  // merges duplicate definitions across DSO boundaries.
  // We also cannot mark it as `const`, otherwise msvc merges all definitions
  // when lto is enabled, making any comparison return true.
  template <typename T> struct TypeId { static char Id; };

  struct StorageBase {
    virtual ~StorageBase() = default;
    virtual Box<StorageBase> clone() const = 0;
    virtual const void* id() const = 0;
  };

  template <typename T> struct StorageImpl : public StorageBase {
    explicit StorageImpl(const T& Value) : Value(Value) {}

    explicit StorageImpl(T&& Value) : Value(std::move(Value)) {}

    Box<StorageBase> clone() const override {
      return std::make_unique<StorageImpl<T>>(Value);
    }

    const void* id() const override { return &TypeId<T>::Id; }

    T Value;

  private:
    StorageImpl &operator=(const StorageImpl &Other) = delete;
    StorageImpl(const StorageImpl &Other) = delete;
  };

public:
  Any() = default;

  Any(const Any &Other)
      : Storage(Other.Storage ? Other.Storage->clone() : nullptr) {}

  // When T is Any or T is not copy-constructible we need to explicitly disable
  // the forwarding constructor so that the copy constructor gets selected
  // instead.
  template <typename T>
	requires any_detail::enable<T>
  Any(T&& Value) {
		using BoxedT = StorageImpl<std::decay_t<T>>;
    Storage = std::make_unique<BoxedT>(EXI_FWD(Value));
  }

  Any(Any&& Other) : Storage(std::move(Other.Storage)) {}

  Any& swap(Any& Other) {
    std::swap(Storage, Other.Storage);
    return *this;
  }

  Any& operator=(Any Other) {
    Storage = std::move(Other.Storage);
    return *this;
  }

  bool has_value() const { return !!Storage; }

  void reset() { Storage.reset(); }

private:
  // Only used for the internal exi::Any implementation
  template <typename T> bool isa() const {
		using U = std::remove_cvref_t<T>;
    if (!Storage)
      return false;
    return Storage->id() == &Any::TypeId<U>::Id;
  }

  template <class T> friend T any_cast(const Any& Value);
  template <class T> friend T any_cast(Any& Value);
  template <class T> friend T any_cast(Any&& Value);
  template <class T> friend const T* any_cast(const Any* Value);
  template <class T> friend T* any_cast(Any* Value);
  template <typename T> friend bool any_isa(const Any& Value);

  Box<StorageBase> Storage;
};

// Define the type id and initialize with a non-zero value.
// Initializing with a zero value means the variable can end up in either the
// .data or the .bss section. This can lead to multiple definition linker errors
// when some object files are compiled with a compiler that puts the variable
// into .data but they are linked to object files from a different compiler that
// put the variable into .bss. To prevent this issue from happening, initialize
// the variable with a non-zero value, which forces it to land in .data (because
// .bss is zero-initialized).
// See also https://github.com/llvm/llvm-project/issues/62270
template <typename T> char Any::TypeId<T>::Id = 1;

template <typename T> EXI_INLINE bool any_isa(const Any& Value) {
	using BaseT = std::remove_cvref_t<T>;
	return Value.isa<BaseT>();
}

template <class T> T any_cast(const Any& Value) {
	using U = std::remove_cvref_t<T>;
  exi_assert(any_isa<T>(Value) && "Bad any cast!");
  return static_cast<T>(*any_cast<U>(&Value));
}

template <class T> T any_cast(Any& Value) {
	using U = std::remove_cvref_t<T>;
  exi_assert(any_isa<T>(Value), "Bad any cast!");
  return static_cast<T>(*any_cast<U>(&Value));
}

template <class T> T any_cast(Any&& Value) {
	using U = std::remove_cvref_t<T>;
  exi_assert(any_isa<T>(Value), "Bad any cast!");
  return static_cast<T>(std::move(*any_cast<U>(&Value)));
}

template <class T> const T* any_cast(const Any* Value) {
  using U = std::remove_cvref_t<T>;
  if (!Value || !any_isa<U>(*Value))
    return nullptr;
  return &static_cast<Any::StorageImpl<U>&>(*Value->Storage).Value;
}

template <class T> T* any_cast(Any* Value) {
  using U = std::decay_t<T>;
  if (!Value || !any_isa<U>(*Value))
    return nullptr;
  return &static_cast<Any::StorageImpl<U>&>(*Value->Storage).Value;
}

} // namespace exi
