//===- Common/STLForwardCompat.hpp ----------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://exi.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file contains library features backported from future STL versions.
///
/// These should be replaced with their STL counterparts as the C++ version LLVM
/// is compiled with is updated.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Features.hpp>
#include <core/Common/D/Detector.hpp>
#include <functional>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace exi {

//===----------------------------------------------------------------------===//
//     Features from C++23
//===----------------------------------------------------------------------===//

// TODO: Remove this in favor of std::optional<T>::transform once we switch to
// C++23.
template <typename Optional, typename Function,
          typename Value = typename std::remove_cvref_t<Optional>::value_type>
constexpr std::optional<std::remove_cvref_t<std::invoke_result_t<Function, Value>>>
transformOptional(Optional &&O, Function &&F) {
  if (O) {
    return std::invoke(std::forward<Function>(F), *std::forward<Optional>(O));
  }
  return std::nullopt;
}

// A tag for constructors accepting ranges.
struct from_range_t {
  explicit from_range_t() = default;
};
inline constexpr from_range_t from_range{};

//===----------------------------------------------------------------------===//
//     Bind functions from C++20 / C++23 / C++26
//===----------------------------------------------------------------------===//

namespace H {
// Tag for constructing with a runtime callable.
struct RuntimeFnTag {};
// Tag for constructing with a compile-time constant callable.
struct ConstantFnTag {};

/// Stores a callable as a data member.
template <typename FnT> struct FnHolder {
  FnT Fn;

  template <typename FnArgT>
  constexpr explicit FnHolder(FnArgT &&F) : Fn(std::forward<FnArgT>(F)) {}

  constexpr FnT &get() { return Fn; }
  constexpr const FnT &get() const { return Fn; }
};

/// Holds a compile-time constant callable (empty storage).
template <auto ConstFn> struct FnConstant {
  constexpr decltype(auto) get() const { return ConstFn; }
};

// Storage class for bind_front/bind_back that properly handles const/non-const
// qualification of the wrapper when invoking the stored callable.
// If BindFront is true, bound args are prepended; otherwise appended.
// FnStorageT is either FnHolder<FnT> (runtime) or FnConstant<ConstFn>.
template <bool BindFront, typename BoundArgsTupleT, typename FnStorageT,
          typename IndicesT>
class BindStorage;

template <bool BindFront, typename BoundArgsTupleT, typename FnStorageT,
          size_t... Indices>
class BindStorage<BindFront, BoundArgsTupleT, FnStorageT,
                  std::index_sequence<Indices...>> {
  BoundArgsTupleT BoundArgs;
  // This may be empty for const functions, hence the `no_unique_address`.
  EXI_NO_UNIQUE_ADDRESS FnStorageT FnStorage;

public:
  // Constructor for FnHolder (runtime callable).
  template <typename FnArgT, typename... BoundArgsArgT>
  constexpr BindStorage(RuntimeFnTag, FnArgT &&F, BoundArgsArgT &&...Args)
      : BoundArgs(std::forward<BoundArgsArgT>(Args)...),
        FnStorage(std::forward<FnArgT>(F)) {}

  // Constructor for FnConstant (compile-time callable).
  template <typename... BoundArgsArgT>
  constexpr BindStorage(ConstantFnTag, BoundArgsArgT &&...Args)
      : BoundArgs(std::forward<BoundArgsArgT>(Args)...), FnStorage() {}

  template <typename... CallArgsT>
  constexpr decltype(auto) operator()(CallArgsT &&...CallArgs) {
    if constexpr (BindFront)
      return std::invoke(FnStorage.get(), std::get<Indices>(BoundArgs)...,
                         std::forward<CallArgsT>(CallArgs)...);
    else
      return std::invoke(FnStorage.get(), std::forward<CallArgsT>(CallArgs)...,
                         std::get<Indices>(BoundArgs)...);
  }

  template <typename... CallArgsT>
  constexpr decltype(auto) operator()(CallArgsT &&...CallArgs) const {
    if constexpr (BindFront)
      return std::invoke(FnStorage.get(), std::get<Indices>(BoundArgs)...,
                         std::forward<CallArgsT>(CallArgs)...);
    else
      return std::invoke(FnStorage.get(), std::forward<CallArgsT>(CallArgs)...,
                         std::get<Indices>(BoundArgs)...);
  }
};
} // namespace H

/// C++20 bind_front. Prepends bound arguments to the callable. All bind
/// arguments and the callable are forwarded and *stored* by value. If you would
/// like to pass by reference, use `std::ref` or `std::cref`.
template <typename FnT, typename... BindArgsT>
constexpr auto bind_front(FnT &&Fn, // NOLINT(readability-identifier-naming)
                          BindArgsT &&...BindArgs) {
  return H::BindStorage</*BindFront=*/true,
                        std::tuple<std::decay_t<BindArgsT>...>,
                        H::FnHolder<std::decay_t<FnT>>,
                        std::index_sequence_for<BindArgsT...>>(
      H::RuntimeFnTag{}, std::forward<FnT>(Fn),
      std::forward<BindArgsT>(BindArgs)...);
}

/// C++26 bind_front with compile-time callable. Prepends bound arguments.
/// Bound arguments are forwarded and *stored* by value.
template <auto ConstFn, typename... BindArgsT>
constexpr auto
bind_front(BindArgsT &&...BindArgs) { // NOLINT(readability-identifier-naming)
  if constexpr (std::is_pointer_v<decltype(ConstFn)> ||
                std::is_member_pointer_v<decltype(ConstFn)>)
    static_assert(ConstFn != nullptr);

  return H::BindStorage<
      /*BindFront=*/true, std::tuple<std::decay_t<BindArgsT>...>,
      H::FnConstant<ConstFn>, std::index_sequence_for<BindArgsT...>>(
      H::ConstantFnTag{}, std::forward<BindArgsT>(BindArgs)...);
}

/// C++23 bind_back. Appends bound arguments to the callable. All bind
/// arguments and the callable are forwarded and *stored* by value. If you would
/// like to pass by reference, use `std::ref` or `std::cref`.
template <typename FnT, typename... BindArgsT>
constexpr auto bind_back(FnT &&Fn, // NOLINT(readability-identifier-naming)
                         BindArgsT &&...BindArgs) {
  return H::BindStorage</*BindFront=*/false,
                        std::tuple<std::decay_t<BindArgsT>...>,
                        H::FnHolder<std::decay_t<FnT>>,
                        std::index_sequence_for<BindArgsT...>>(
      H::RuntimeFnTag{}, std::forward<FnT>(Fn),
      std::forward<BindArgsT>(BindArgs)...);
}

/// C++26 bind_back with compile-time callable. Appends bound arguments.
/// Bound arguments are forwarded and *stored* by value.
template <auto ConstFn, typename... BindArgsT>
constexpr auto
bind_back(BindArgsT &&...BindArgs) { // NOLINT(readability-identifier-naming)
  if constexpr (std::is_pointer_v<decltype(ConstFn)> ||
                std::is_member_pointer_v<decltype(ConstFn)>)
    static_assert(ConstFn != nullptr);

  return H::BindStorage<
      /*BindFront=*/false, std::tuple<std::decay_t<BindArgsT>...>,
      H::FnConstant<ConstFn>, std::index_sequence_for<BindArgsT...>>(
      H::ConstantFnTag{}, std::forward<BindArgsT>(BindArgs)...);
}
} // namespace exi
