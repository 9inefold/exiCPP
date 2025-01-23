//===- ArrayRef.hpp -------------------------------------------------===//
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
/// Stripped down version of exi::ArrayRef.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>
#include <initializer_list>
#include <type_traits>

namespace re {
template <typename T> class [[nodiscard]] ArrayRef;
template <typename T> class [[nodiscard]] MutArrayRef;

template <typename T> 
class ArrayRefBase {
  using Derived = ArrayRef<T>;
public:
  constexpr ArrayRefBase() = default;

  /// equals - Check for element-wise equality.
  bool equals(ArrayRef<T> RHS) const {
    if (self().Length != RHS.Length)
      return false;
    auto It = RHS.begin();
    for (const T& Val : self()) {
      if (Val != *It)
        return false;
      ++It;
    }
    return true;
  }

private:
  const Derived& self() const {
    return *static_cast<const Derived*>(this);
  }
};

template <> class ArrayRefBase<char> {
public:
  constexpr ArrayRefBase() = default;

  /// equals - Check for element-wise equality.
  bool equals(ArrayRef<char> RHS) const;

  /// Check if this string starts with the given \p Prefix.
  [[nodiscard]] bool starts_with(ArrayRef<char> Prefix) const;
  [[nodiscard]] bool starts_with(char Prefix) const;

  /// Check if this string starts with the given \p Prefix, ignoring case.
  [[nodiscard]] bool starts_with_insensitive(ArrayRef<char> Prefix) const;

  /// Check if this string ends with the given \p Suffix.
  [[nodiscard]] bool ends_with(ArrayRef<char> Suffix) const;
  [[nodiscard]] bool ends_with(char Suffix) const;

  /// Check if this string ends with the given \p Suffix, ignoring case.
  [[nodiscard]] bool ends_with_insensitive(ArrayRef<char> Suffix) const;

private:
  bool isGrEqual(ArrayRef<char> RHS) const;
  const ArrayRef<char>& self() const;
};

/// ArrayRef - Represent a constant reference to an array (0 or more elements
/// consecutively in memory), i.e. a start pointer and a length.  It allows
/// various APIs to take consecutive elements easily and conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the ArrayRef. For this reason, it is not in general
/// safe to store an ArrayRef.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <typename T>
class [[nodiscard]] ArrayRef : public ArrayRefBase<T> {
  friend class ArrayRefBase<T>;
public:
  using value_type = T;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using reference = value_type &;
  using const_reference = const value_type &;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using size_type = usize;
  using difference_type = ptrdiff_t;

private:
  /// The start of the array, in an external buffer.
  const T *Data = nullptr;

  /// The number of elements.
  size_type Length = 0;

public:
  /// @name Constructors
  /// @{

  /// Construct an empty ArrayRef.
  /*implicit*/ ArrayRef() = default;

  /// Construct an ArrayRef from a single element.
  /*implicit*/ ArrayRef(const T &OneElt)
    : Data(&OneElt), Length(1) {}

  /// Construct an ArrayRef from a pointer and length.
  constexpr /*implicit*/ ArrayRef(const T *data, usize length)
    : Data(data), Length(length) {}

  /// Construct an ArrayRef from a range.
  constexpr ArrayRef(const T *begin, const T *end)
      : Data(begin), Length(end - begin) {
    re_assert(begin <= end);
  }

  /// Construct an ArrayRef from a C array.
  template <usize N>
  /*implicit*/ constexpr ArrayRef(const T (&Arr)[N]) : Data(Arr), Length(N) {}

  /// Construct an ArrayRef from a std::initializer_list.
#if defined(__GNUC__) && !defined(__clang__)
// Disable gcc's warning in this constructor as it generates an enormous amount
// of messages. Anyone using ArrayRef should already be aware of the fact that
// it does not do lifetime extension.
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Winit-list-lifetime"
#endif
  constexpr /*implicit*/ ArrayRef(const std::initializer_list<T> &Vec)
      : Data(Vec.begin() == Vec.end() ? (T *)nullptr : Vec.begin()),
        Length(Vec.size()) {}
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
#endif

  /// Construct an ArrayRef<const T*> from ArrayRef<T*>. This uses SFINAE to
  /// ensure that only ArrayRefs of pointers can be converted.
  template <typename U>
  ArrayRef(const ArrayRef<U *> &A,
           std::enable_if_t<std::is_convertible<U *const *, T const *>::value>
               * = nullptr)
      : Data(A.data()), Length(A.size()) {}

  /// @}
  /// @name Simple Operations
  /// @{

  iterator begin() const { return Data; }
  iterator end() const { return Data + Length; }

  /// empty - Check if the array is empty.
  bool empty() const { return Length == 0; }
  explicit operator bool() const { return !empty(); }

  const T *data() const { return Data; }

  /// size - Get the array size.
  usize size() const { return Length; }

  /// front - Get the first element.
  const T &front() const {
    re_assert(!empty());
    return Data[0];
  }

  /// back - Get the last element.
  const T &back() const {
    re_assert(!empty());
    return Data[Length-1];
  }

  /// slice(n, m) - Chop off the first N elements of the array, and keep M
  /// elements in the array.
  ArrayRef<T> slice(usize N, usize M) const {
    re_assert(N+M <= size(), "Invalid specifier");
    return ArrayRef<T>(data()+N, M);
  }

  /// slice(n) - Chop off the first N elements of the array.
  ArrayRef<T> slice(usize N) const { return slice(N, size() - N); }

  /// Drop the first \p N elements of the array.
  ArrayRef<T> drop_front(usize N = 1) const {
    re_assert(size() >= N, "Dropping more elements than exist");
    return slice(N, size() - N);
  }

  /// Drop the last \p N elements of the array.
  ArrayRef<T> drop_back(usize N = 1) const {
    re_assert(size() >= N, "Dropping more elements than exist");
    return slice(0, size() - N);
  }

  /// Return a copy of *this with only the first \p N elements.
  ArrayRef<T> take_front(usize N = 1) const {
    if (N >= size())
      return *this;
    return drop_back(size() - N);
  }

  /// Return a copy of *this with only the last \p N elements.
  ArrayRef<T> take_back(usize N = 1) const {
    if (N >= size())
      return *this;
    return drop_front(size() - N);
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  const T &operator[](usize Index) const {
    re_assert(Index < Length, "Invalid index!");
    return Data[Index];
  }

  /// Disallow accidental assignment from a temporary.
  ///
  /// The declaration here is extra complicated so that "arrayRef = {}"
  /// continues to select the move assignment operator.
  template <typename U>
  std::enable_if_t<std::is_same<U, T>::value, ArrayRef<T>> &
  operator=(U &&Temporary) = delete;

  /// Disallow accidental assignment from a temporary.
  ///
  /// The declaration here is extra complicated so that "arrayRef = {}"
  /// continues to select the move assignment operator.
  template <typename U>
  std::enable_if_t<std::is_same<U, T>::value, ArrayRef<T>> &
  operator=(std::initializer_list<U>) = delete;

  /// @}
};

// template <typename T>
// inline bool ArrayRefBase<T>::equals(ArrayRef<T> RHS) const {
//   if (self().Length != RHS.Length)
//     return false;
//   auto It = RHS.begin();
//   for (const T& Val : self()) {
//     if (Val != *It)
//       return false;
//     ++It;
//   }
//   return true;
// }

/// MutArrayRef - Represent a mutable reference to an array (0 or more
/// elements consecutively in memory), i.e. a start pointer and a length.  It
/// allows various APIs to take and modify consecutive elements easily and
/// conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the MutArrayRef. For this reason, it is not in
/// general safe to store a MutArrayRef.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <typename T>
class [[nodiscard]] MutArrayRef : public ArrayRef<T> {
public:
  using value_type = T;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using reference = value_type &;
  using const_reference = const value_type &;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using size_type = usize;
  using difference_type = ptrdiff_t;

  /// Construct an empty MutArrayRef.
  /*implicit*/ MutArrayRef() = default;

  /// Construct a MutArrayRef from a single element.
  /*implicit*/ MutArrayRef(T &OneElt) : ArrayRef<T>(OneElt) {}

  /// Construct a MutArrayRef from a pointer and length.
  /*implicit*/ MutArrayRef(T *data, usize length)
    : ArrayRef<T>(data, length) {}

  /// Construct a MutArrayRef from a range.
  MutArrayRef(T *begin, T *end) : ArrayRef<T>(begin, end) {}

  /// Construct a MutArrayRef from a C array.
  template <usize N>
  /*implicit*/ constexpr MutArrayRef(T (&Arr)[N]) : ArrayRef<T>(Arr) {}

  T *data() const { return const_cast<T*>(ArrayRef<T>::data()); }

  iterator begin() const { return data(); }
  iterator end() const { return data() + this->size(); }

  /// front - Get the first element.
  T &front() const {
    re_assert(!this->empty());
    return data()[0];
  }

  /// back - Get the last element.
  T &back() const {
    re_assert(!this->empty());
    return data()[this->size()-1];
  }

  /// slice(n, m) - Chop off the first N elements of the array, and keep M
  /// elements in the array.
  MutArrayRef<T> slice(usize N, usize M) const {
    re_assert(N + M <= this->size(), "Invalid specifier");
    return MutArrayRef<T>(this->data() + N, M);
  }

  /// slice(n) - Chop off the first N elements of the array.
  MutArrayRef<T> slice(usize N) const {
    return slice(N, this->size() - N);
  }

  /// Drop the first \p N elements of the array.
  MutArrayRef<T> drop_front(usize N = 1) const {
    re_assert(this->size() >= N, "Dropping more elements than exist");
    return slice(N, this->size() - N);
  }

  MutArrayRef<T> drop_back(usize N = 1) const {
    re_assert(this->size() >= N, "Dropping more elements than exist");
    return slice(0, this->size() - N);
  }

  /// Return a copy of *this with only the first \p N elements.
  MutArrayRef<T> take_front(usize N = 1) const {
    if (N >= this->size())
      return *this;
    return drop_back(this->size() - N);
  }

  /// Return a copy of *this with only the last \p N elements.
  MutArrayRef<T> take_back(usize N = 1) const {
    if (N >= this->size())
      return *this;
    return drop_front(this->size() - N);
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  T &operator[](usize Index) const {
    re_assert(Index < this->size(), "Invalid index!");
    return data()[Index];
  }
};

/// @name ArrayRef Deduction guides
/// @{
/// Deduction guide to construct an ArrayRef from a single element.
template <typename T> ArrayRef(const T &OneElt) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a pointer and length
template <typename T> ArrayRef(const T *data, usize length) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a range
template <typename T> ArrayRef(const T *data, const T *end) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from an ArrayRef (const)
template <typename T> ArrayRef(const ArrayRef<T> &Vec) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from an ArrayRef
template <typename T> ArrayRef(ArrayRef<T> &Vec) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a C array.
template <typename T, usize N> ArrayRef(const T (&Arr)[N]) -> ArrayRef<T>;

/// @}

/// @name MutArrayRef Deduction guides
/// @{
/// Deduction guide to construct a `MutArrayRef` from a single element
template <class T> MutArrayRef(T &OneElt) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a pointer and length.
template <class T> MutArrayRef(T *data, usize length) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a pointer and length.
template <class T> MutArrayRef(T *data, T *end) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a C array.
template <typename T, usize N>
MutArrayRef(T (&Arr)[N]) -> MutArrayRef<T>;

/// @}
/// @name ArrayRef Comparison Operators
/// @{

template <typename T>
inline bool operator==(ArrayRef<T> LHS, ArrayRef<T> RHS) {
  return LHS.equals(RHS);
}

template <typename T>
inline bool operator!=(ArrayRef<T> LHS, ArrayRef<T> RHS) {
  return !(LHS == RHS);
}

/// @}



} // namespace re
