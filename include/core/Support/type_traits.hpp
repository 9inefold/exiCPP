//===- Support/type_traits.hpp --------------------------------------===//
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
//
// This file provides useful additions to the standard type_traits library.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <type_traits>
#include <utility>

namespace exi {

/// Metafunction that determines whether the given type is either an
/// integral type or an enumeration type, including enum classes.
///
/// Note that this accepts potentially more integral types than is_integral
/// because it is based on being implicitly convertible to an integral type.
/// Also note that enum classes aren't implicitly convertible to integral types,
/// the value may therefore need to be explicitly converted before being used.
template <typename T> class is_integral_or_enum {
  using UnderlyingT = std::remove_reference_t<T>;
public:
  static const bool value = // Filter conversion operators.
    !std::is_class_v<UnderlyingT>
    && !std::is_pointer_v<UnderlyingT>
    && !std::is_floating_point_v<UnderlyingT>
    && (std::is_enum_v<UnderlyingT> ||
      std::is_convertible_v<UnderlyingT, unsigned long long>);
};

/// If T is a pointer, just return it. If it is not, return T&.
template <typename T, typename Enable = void>
struct add_lvalue_reference_if_not_pointer { using type = T&; };

template <typename T>
struct add_lvalue_reference_if_not_pointer<
 T, std::enable_if_t<std::is_pointer_v<T>>> {
  using type = T;
};

/// If T is a pointer to X, return a pointer to const X. If it is not,
/// return const T.
template <typename T, typename Enable = void>
struct add_const_past_pointer { using type = const T; };

template <typename T>
struct add_const_past_pointer<T, std::enable_if_t<std::is_pointer_v<T>>> {
  using type = const std::remove_pointer_t<T>*;
};

template <typename T, typename Enable = void>
struct const_pointer_or_const_ref {
  using type = const T&;
};

template <typename T>
struct const_pointer_or_const_ref<T, std::enable_if_t<std::is_pointer_v<T>>> {
  using type = typename add_const_past_pointer<T>::type;
};

namespace H {

template <class T>
union trivial_helper { T t; };

} // end namespace H

template <typename T>
struct is_copy_assignable {
  template <class F> static auto get(F*)
   -> decltype(std::declval<F&>() = std::declval<const F&>(), std::true_type{});
  static std::false_type get(...);
  static constexpr bool value = decltype(get((T*)nullptr))::value;
};

template <typename T>
struct is_move_assignable {
  template <class F> static auto get(F*)
   -> decltype(std::declval<F&>() = std::declval<F&&>(), std::true_type{});
  static std::false_type get(...);
  static constexpr bool value = decltype(get((T*)nullptr))::value;
};

//////////////////////////////////////////////////////////////////////////
// Globals

/// Metafunction that determines whether the given type is either an
/// integral type or an enumeration type, including enum classes.
///
/// See `is_integral_or_enum` for more info.
template <typename T
EXI_CONST bool is_integral_or_enum_v
  = is_integral_or_enum<T>::value;

/// If T is a pointer, just return it. If it is not, return T&.
template <typename T>
using add_lvalue_reference_if_not_pointer_t
  = typename add_lvalue_reference_if_not_pointer<T>::type;

/// If T is a pointer to X, return a pointer to const X. If it is not,
/// return const T.
template <typename T>
using add_const_past_pointer_t
  = typename add_const_past_pointer<T>::type;

template <typename T>
using const_pointer_or_const_ref_t
  = typename const_pointer_or_const_ref<T>::type;

template <typename T>
EXI_CONST bool is_copy_assignable_v
  = is_copy_assignable<T>::value;

template <typename T>
EXI_CONST bool is_move_assignable_v
  = is_move_assignable<T>::value;

} // namespace exi
