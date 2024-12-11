//===- Common/iterator_range.hpp ------------------------------------===//
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
/// This provides a very simple, boring adaptor for a begin and end iterator
/// into a range type. This should be used to build range views that work well
/// with range based for loops and range based constructors.
///
/// Note that code here follows more standards-based coding conventions as it
/// is mirroring proposed interfaces for standardization.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ADL.hpp>
#include <type_traits>
#include <utility>

namespace exi {

template <typename From, typename To, typename = void>
struct explicitly_convertible : std::false_type {};

template <typename From, typename To>
struct explicitly_convertible<
    From, To,
    std::void_t<decltype(static_cast<To>(
        std::declval<std::add_rvalue_reference_t<From>>()))>> : std::true_type {
};

/// A range adaptor for a pair of iterators.
///
/// This just wraps two iterators into a range-compatible interface. Nothing
/// fancy at all.
template <typename IteratorT>
class iterator_range {
  IteratorT begin_iterator, end_iterator;

public:
#if defined(__GNUC__) &&                                                       \
    (__GNUC__ == 7 || (__GNUC__ == 8 && __GNUC_MINOR__ < 4))
  // Be careful no to break gcc-7 and gcc-8 < 8.4 on the mlir target.
  // See https://github.com/llvm/llvm-project/issues/63843
  template <typename Container>
#else
  template <
      typename Container,
      std::enable_if_t<explicitly_convertible<
          exi::H::IterOfRange<Container>, IteratorT>::value> * = nullptr>
#endif
  iterator_range(Container &&c)
      : begin_iterator(adl_begin(c)), end_iterator(adl_end(c)) {
  }
  iterator_range(IteratorT begin_iterator, IteratorT end_iterator)
      : begin_iterator(std::move(begin_iterator)),
        end_iterator(std::move(end_iterator)) {}

  IteratorT begin() const { return begin_iterator; }
  IteratorT end() const { return end_iterator; }
  bool empty() const { return begin_iterator == end_iterator; }
};

template <typename Container>
iterator_range(Container &&)
    -> iterator_range<exi::H::IterOfRange<Container>>;

/// Convenience function for iterating over sub-ranges.
///
/// This provides a bit of syntactic sugar to make using sub-ranges
/// in for loops a bit easier. Analogous to std::make_pair().
template <class T> iterator_range<T> make_range(T x, T y) {
  return iterator_range<T>(std::move(x), std::move(y));
}

template <typename T> iterator_range<T> make_range(std::pair<T, T> p) {
  return iterator_range<T>(std::move(p.first), std::move(p.second));
}

} // namespace exi
