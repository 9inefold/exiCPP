//===- Support/PointerLikeTraits.hpp ---------------------------------===//
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
/// This file defines the PointerLikeTypeTraits class.  This allows data
/// structures to reason about pointers and other things that are pointer sized.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ConstexprLists.hpp>
#include <Support/ErrorHandle.hpp>
#include <cassert>
#include <type_traits>

namespace exi {

/// A traits type that is used to handle pointer types and things that are just
/// wrappers for pointers as a uniform entity.
template <typename T> struct PointerLikeTypeTraits;

namespace H {
/// A tiny meta function to compute the log2 of a compile time constant.
template <usize N>
struct ConstantLog2 : idx_c<ConstantLog2<N / 2>::value + 1> {};
template <> struct ConstantLog2<1> : idx_c<0> {};

// Provide a trait to check if T is pointer-like.
template <typename T, typename U = void> struct HasPointerLikeTypeTraits {
  static constexpr bool value = false;
};

// sizeof(T) is valid only for a complete T.
template <typename T>
struct HasPointerLikeTypeTraits<
    T, decltype((sizeof(PointerLikeTypeTraits<T>) + sizeof(T)), void())> {
  static constexpr bool value = true;
};

template <typename T> struct IsPointerLike {
  static constexpr bool value = HasPointerLikeTypeTraits<T>::value;
};

template <typename T> struct IsPointerLike<T *> {
  static constexpr bool value = true;
};

} // namespace H

/// This concept checks if `PointerLikeTypeTraits` has been specialized for `T`.
template <typename T>
concept has_pointerlike_type_traits = H::HasPointerLikeTypeTraits<T>::value;

/// This concept checks if `T` is a pointer OR has traits.
template <typename T>
concept is_pointerlike = H::IsPointerLike<T>::value;

/// This concept checks if `T` is a pointer OR has traits, but is not a `uptr.
/// This is useful knowledge when you are accepting arbitrary objects, but don't
/// want to randomly convert integers to `void*`.
template <typename T>
concept is_pointerlike_and_not_uptr
  = !std::same_as<std::remove_const_t<T>, uptr> && is_pointerlike<T>;

//////////////////////////////////////////////////////////////////////////

// Provide PointerLikeTypeTraits for non-cvr pointers.
template <typename T> struct PointerLikeTypeTraits<T *> {
  static inline void *getAsVoidPointer(T *P) { return P; }
  static inline T *getFromVoidPointer(void *P) { return static_cast<T *>(P); }

  static constexpr int NumLowBitsAvailable
    = H::ConstantLog2<alignof(T)>::value;
};

template <> struct PointerLikeTypeTraits<void *> {
  static inline void *getAsVoidPointer(void *P) { return P; }
  static inline void *getFromVoidPointer(void *P) { return P; }

  /// Note, we assume here that void* is related to raw malloc'ed memory and
  /// that malloc returns objects at least 4-byte aligned. However, this may be
  /// wrong, or pointers may be from something other than malloc. In this case,
  /// you should specify a real typed pointer or avoid this template.
  ///
  /// All clients should use assertions to do a run-time check to ensure that
  /// this is actually true.
  static constexpr int NumLowBitsAvailable = 2;
};

// Provide `PointerLikeTypeTraits` for const things.
template <typename T> struct PointerLikeTypeTraits<const T> {
  using NonConst = PointerLikeTypeTraits<T>;

  static inline const void *getAsVoidPointer(const T P) {
    return NonConst::getAsVoidPointer(P);
  }
  static inline const T getFromVoidPointer(const void *P) {
    return NonConst::getFromVoidPointer(const_cast<void *>(P));
  }
  static constexpr int NumLowBitsAvailable = NonConst::NumLowBitsAvailable;
};

// Provide PointerLikeTypeTraits for const pointers.
template <typename T> struct PointerLikeTypeTraits<const T *> {
  using NonConst = PointerLikeTypeTraits<T *>;

  static inline const void *getAsVoidPointer(const T *P) {
    return NonConst::getAsVoidPointer(const_cast<T *>(P));
  }
  static inline const T *getFromVoidPointer(const void *P) {
    return NonConst::getFromVoidPointer(const_cast<void *>(P));
  }
  static constexpr int NumLowBitsAvailable = NonConst::NumLowBitsAvailable;
};

// Provide PointerLikeTypeTraits for uptr.
template <> struct PointerLikeTypeTraits<uptr> {
  static inline void *getAsVoidPointer(uptr P) {
    return reinterpret_cast<void *>(P);
  }
  static inline uptr getFromVoidPointer(void *P) {
    return reinterpret_cast<uptr>(P);
  }
  // No bits are available!
  static constexpr int NumLowBitsAvailable = 0;
};

/// Provide suitable custom traits struct for function pointers.
///
/// Function pointers can't be directly given these traits as functions can't
/// have their alignment computed with `alignof` and we need different casting.
///
/// To rely on higher alignment for a specialized use, you can provide a
/// customized form of this template explicitly with higher alignment, and
/// potentially use alignment attributes on functions to satisfy that.
template <int Alignment, typename FunctionPointerT>
struct FunctionPointerLikeTypeTraits {
  static constexpr int NumLowBitsAvailable =
      H::ConstantLog2<Alignment>::value;
  static inline void *getAsVoidPointer(FunctionPointerT P) {
    exi_assert((reinterpret_cast<uptr>(P) &
            ~(uptr(-1) << NumLowBitsAvailable)) == 0,
           "Alignment not satisfied for an actual function pointer!");
    return reinterpret_cast<void *>(P);
  }
  static inline FunctionPointerT getFromVoidPointer(void *P) {
    return reinterpret_cast<FunctionPointerT>(P);
  }
};

/// Provide a default specialization for function pointers that assumes 4-byte
/// alignment.
///
/// We assume here that functions used with this are always at least 4-byte
/// aligned. This means that, for example, thumb functions won't work or systems
/// with weird unaligned function pointers won't work. But all practical systems
/// we support satisfy this requirement.
template <typename ReturnT, typename... ParamTs>
struct PointerLikeTypeTraits<ReturnT (*)(ParamTs...)>
    : FunctionPointerLikeTypeTraits<4, ReturnT (*)(ParamTs...)> {};

} // namespace exi
