//===- Common/ADL.hpp -----------------------------------------------===//
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

#pragma once

#include <type_traits>
#include <iterator>
#include <utility>

namespace exi {

// Only used by compiler if both template types are the same.  Useful when
// using SFINAE to test for the existence of member functions.
template <typename T, T> struct SameType;

namespace adl_detail {

using std::begin;

template <typename RangeT>
constexpr auto begin_impl(RangeT &&range)
    -> decltype(begin(std::forward<RangeT>(range))) {
  return begin(std::forward<RangeT>(range));
}

using std::end;

template <typename RangeT>
constexpr auto end_impl(RangeT &&range)
    -> decltype(end(std::forward<RangeT>(range))) {
  return end(std::forward<RangeT>(range));
}

using std::rbegin;

template <typename RangeT>
constexpr auto rbegin_impl(RangeT &&range)
    -> decltype(rbegin(std::forward<RangeT>(range))) {
  return rbegin(std::forward<RangeT>(range));
}

using std::rend;

template <typename RangeT>
constexpr auto rend_impl(RangeT &&range)
    -> decltype(rend(std::forward<RangeT>(range))) {
  return rend(std::forward<RangeT>(range));
}

using std::swap;

template <typename T>
constexpr void swap_impl(T &&lhs,
                         T &&rhs) noexcept(noexcept(swap(std::declval<T>(),
                                                         std::declval<T>()))) {
  swap(std::forward<T>(lhs), std::forward<T>(rhs));
}

using std::size;

template <typename RangeT>
constexpr auto size_impl(RangeT &&range)
    -> decltype(size(std::forward<RangeT>(range))) {
  return size(std::forward<RangeT>(range));
}

} // namespace adl_detail

/// Returns the begin iterator to \p range using `std::begin` and
/// function found through Argument-Dependent Lookup (ADL).
template <typename RangeT>
constexpr auto adl_begin(RangeT &&range)
    -> decltype(adl_detail::begin_impl(std::forward<RangeT>(range))) {
  return adl_detail::begin_impl(std::forward<RangeT>(range));
}

/// Returns the end iterator to \p range using `std::end` and
/// functions found through Argument-Dependent Lookup (ADL).
template <typename RangeT>
constexpr auto adl_end(RangeT &&range)
    -> decltype(adl_detail::end_impl(std::forward<RangeT>(range))) {
  return adl_detail::end_impl(std::forward<RangeT>(range));
}

/// Returns the reverse-begin iterator to \p range using `std::rbegin` and
/// function found through Argument-Dependent Lookup (ADL).
template <typename RangeT>
constexpr auto adl_rbegin(RangeT &&range)
    -> decltype(adl_detail::rbegin_impl(std::forward<RangeT>(range))) {
  return adl_detail::rbegin_impl(std::forward<RangeT>(range));
}

/// Returns the reverse-end iterator to \p range using `std::rend` and
/// functions found through Argument-Dependent Lookup (ADL).
template <typename RangeT>
constexpr auto adl_rend(RangeT &&range)
    -> decltype(adl_detail::rend_impl(std::forward<RangeT>(range))) {
  return adl_detail::rend_impl(std::forward<RangeT>(range));
}

/// Swaps \p lhs with \p rhs using `std::swap` and functions found through
/// Argument-Dependent Lookup (ADL).
template <typename T>
constexpr void adl_swap(T &&lhs, T &&rhs) noexcept(
    noexcept(adl_detail::swap_impl(std::declval<T>(), std::declval<T>()))) {
  adl_detail::swap_impl(std::forward<T>(lhs), std::forward<T>(rhs));
}

/// Returns the size of \p range using `std::size` and functions found through
/// Argument-Dependent Lookup (ADL).
template <typename RangeT>
constexpr auto adl_size(RangeT &&range)
    -> decltype(adl_detail::size_impl(std::forward<RangeT>(range))) {
  return adl_detail::size_impl(std::forward<RangeT>(range));
}

namespace H {

template <typename RangeT>
using IterOfRange = decltype(adl_begin(std::declval<RangeT &>()));

template <typename RangeT>
using ValueOfRange =
    std::remove_reference_t<decltype(*adl_begin(std::declval<RangeT &>()))>;

} // namespace H
} // namespace exi
