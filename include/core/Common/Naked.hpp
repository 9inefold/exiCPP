//===- Common/QualTraits.hpp ----------------------------------------===//
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
/// This file defines a wrapper for pointer types to ensure they aren't
/// used in an invalid/null state.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/DenseMapInfo.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/PointerLikeTraits.hpp>
#include <compare>

namespace exi {

/// A pointer wrapper, useful when you want to assert a pointer is be valid
/// on use. In release it works the same as a normal pointer.
template <typename T> class Naked {
	using StrongOrd = std::strong_ordering;
  // using WeakOrd = std::weak_ordering;
	T* Data = nullptr;
public:
  ALWAYS_INLINE constexpr Naked() = default;
  ALWAYS_INLINE constexpr Naked(T* Ptr) : Data(Ptr) { }

  ALWAYS_INLINE constexpr Naked(const Naked&) = default;
  template <typename U>
	constexpr Naked(const Naked<U>& O) : Data(O.data()) { }

  ALWAYS_INLINE constexpr Naked(Naked&&) = default;
  template <typename U>
	constexpr Naked(Naked<U>&& O) : Data(O.data()) { }

	constexpr Naked& operator=(T* Ptr) {
		Data = Ptr;
		return *this;
	}

	constexpr Naked& operator=(const Naked&) = default;
  template <typename U>
	constexpr Naked& operator=(const Naked<U>& O) {
		Data = O.data();
		return *this;
	}

  constexpr Naked& operator=(Naked&&) = default;
  template <typename U>
	constexpr Naked& operator=(Naked<U>&& O) {
		Data = O.data();
		return *this;
	}

  ALWAYS_INLINE constexpr T* get() const { return Data; }
  ALWAYS_INLINE constexpr T* data() const { return Data; }
  ALWAYS_INLINE constexpr void clear() { Data = nullptr; }

  EXI_INLINE constexpr T& operator*() const {
		exi_assert(Data);
		return *Data;
	}
  EXI_INLINE constexpr T* operator->() const {
		exi_assert(Data);
		return Data;
	}

  EXI_INLINE constexpr operator T*() { return Data; }

  EXI_INLINE constexpr bool operator!() const { return !Data; }
  EXI_INLINE constexpr explicit operator bool() const { return !!Data; }

  constexpr void swap(Naked& O) {
		std::swap(Data, O.Data);
	}

	constexpr bool operator==(const Naked&) const = default;
  constexpr StrongOrd operator<=>(const Naked&) const = default;

	template <typename U>
	constexpr bool operator==(const Naked<U>& O) const {
		return this->Data == O.data();
	}
	template <typename U>
  constexpr StrongOrd operator<=>(const Naked<U>& O) const {
		return this->Data <=> O.data();
	}

  constexpr bool operator==(T* Ptr) const {
    return this->Data == Ptr;
  }
  constexpr StrongOrd operator<=>(T* Ptr) const {
    return this->Data <=> Ptr;
  }
};

template <typename T> struct PointerLikeTypeTraits<Naked<T>> {
	using TraitsT = PointerLikeTypeTraits<T*>;

  static inline void* getAsVoidPointer(const Naked<T>& Ptr) {
    return TraitsT::getAsVoidPointer(Ptr.data());
  }

  static inline Naked<T> getFromVoidPointer(void* Ptr) {
    return TraitsT::getFromVoidPointer(Ptr);
  }

  static constexpr int NumLowBitsAvailable = TraitsT::NumLowBitsAvailable;
};

template <typename T> struct DenseMapInfo<Naked<T>> {
  static inline Naked<T> getEmptyKey() {
		return DenseMapInfo<T*>::getEmptyKey();
	}

  static inline Naked<T> getTombstoneKey() {
    return DenseMapInfo<T*>::getTombstoneKey();
  }

  static unsigned getHashValue(const Naked<T>& UnionVal) {
    const intptr_t key = (intptr_t)T.get();
    return DenseMapInfo<intptr_t>::getHashValue(key);
  }

  static bool isEqual(const Naked<T>& LHS, const Naked<T>& RHS) {
    return LHS == RHS;
  }
};

} // namespace exi
