//===- Common/Array.hpp ---------------------------------------------===//
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
///  This file provides an Array type.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Support/ErrorHandle.hpp>
#include <iterator>
#include <type_traits>

namespace exi {

template <typename T> class ArrayRef;

#if 0
template <typename T, usize N>
struct Array {
	static constexpr usize Length = N; 
	T Data[Length];
public:
	using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = usize;
  using difference_type = ptrdiff_t;

	/// size - Get the array size.
  static constexpr usize size() { return Length; }
	/// size - Get the array size.
  static constexpr bool empty() { return false; }

	constexpr bool operator==(const Array&) const = default;

	template <usize N>
	constexpr bool operator==(const Array<T, N>&) const {
		return false;
	}

	/// equals - Check for element-wise equality.
	constexpr bool equals(const Array& RHS) const {
		return this->operator==(RHS);
	}

	/// equals - Check for element-wise equality.
	template <usize N>
	constexpr bool equals(const Array<T, N>& RHS) const {
		return false;
	}

	////////////////////////////////////////////////////////////////////////

	constexpr T* data() { return Data; }
	constexpr const T* data() const { return Data; }

	/// front - Get the first element.
  constexpr T& front() { return Data[0]; }
  /// front - Get the first element.
  constexpr const T& front() const { return Data[0]; }

	/// back - Get the last element.
  constexpr T& back() { return Data[Length - 1]; }
  /// back - Get the last element.
  constexpr const T& back() const { return Data[Length - 1]; }

	////////////////////////////////////////////////////////////////////////

	constexpr iterator begin() { return Data; }
  constexpr iterator end() { return Data + N; }

	constexpr const_iterator begin() const { return Data; }
  constexpr const_iterator end() const { return Data + N; }

	reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const {
		return const_reverse_iterator(end());
	}
  const_reverse_iterator rend() const {
		return const_reverse_iterator(begin());
	}

	////////////////////////////////////////////////////////////////////////

	constexpr T& operator[](usize Ix) {
    exi_assert(Ix < Length, "Invalid index!");
    return Data[Ix];
  }
	constexpr const T& operator[](usize Ix) const {
    exi_assert(Ix < Length, "Invalid index!");
    return Data[Ix];
  }
};

template <typename T, typename...TT>
Array(T&&, TT&&...) -> Array<std::decay_t<T>, sizeof...(TT) + 1>;
#else
template <typename T, usize N>
using Array = std::array<T, N>;
#endif

} // namespace exi
