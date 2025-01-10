//===- Common/ArrayRef.hpp ------------------------------------------===//
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

#include <Common/Features.hpp>
#include <Common/Hashing.hpp>
#include <Common/SmallVec.hpp>
#include <Common/Vec.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <memory>
#include <span>
#include <type_traits>

namespace exi {
template <typename T> class [[nodiscard]] MutArrayRef;

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
class EXI_GSL_POINTER [[nodiscard]] ArrayRef {
public:
  using value_type = T;
  using pointer = value_type *;
  using const_pointer = const value_type *;
  using reference = value_type &;
  using const_reference = const value_type &;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
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

  /// Construct an empty ArrayRef from std::nullopt.
  /*implicit*/ ArrayRef(std::nullopt_t) {}

  /// Construct an ArrayRef from a single element.
  /*implicit*/ ArrayRef(const T &OneElt)
    : Data(&OneElt), Length(1) {}

  /// Construct an ArrayRef from a pointer and length.
  constexpr /*implicit*/ ArrayRef(const T *data, usize length)
    : Data(data), Length(length) {}

  /// Construct an ArrayRef from a range.
  constexpr ArrayRef(const T *begin, const T *end)
      : Data(begin), Length(end - begin) {
    assert(begin <= end);
  }

  /// Construct an ArrayRef from a SmallVec. This is templated in order to
    /// avoid instantiating SmallVecTemplateCommon<T> whenever we
    /// copy-construct an ArrayRef.
  template <typename U>
  /*implicit*/ ArrayRef(const SmallVecTemplateCommon<T, U> &Vec)
    : Data(Vec.data()), Length(Vec.size()) {}

  /// Construct an ArrayRef from a Vec.
  template <typename A>
  /*implicit*/ ArrayRef(const Vec<T, A> &Vec)
    : Data(Vec.data()), Length(Vec.size()) {}
  
  /// Construct an ArrayRef from a std::span.
  template <usize Ext>
  /*implicit*/ constexpr ArrayRef(std::span<T, Ext> Sp)
    : Data(Sp.data()), Length(Sp.size()) {}

  /// Construct an ArrayRef from a std::array.
  template <usize N>
  /*implicit*/ constexpr ArrayRef(const std::array<T, N> &Arr)
    : Data(Arr.data()), Length(N) {}

  /// Construct an ArrayRef from a C array.
  template <usize N>
  /*implicit*/ constexpr ArrayRef(const T (&Arr)[N]) : Data(Arr), Length(N) {}

  // TODO: Add SmallVec...

  /// Construct an ArrayRef from a std::initializer_list.
DIAGNOSTIC_PUSH()
GCC_IGNORED("-Winit-list-lifetime")
// Disable gcc's warning in this constructor as it generates an enormous amount
// of messages. Anyone using ArrayRef should already be aware of the fact that
// it does not do lifetime extension.
  constexpr /*implicit*/ ArrayRef(const std::initializer_list<T> &Vec)
      : Data(Vec.begin() == Vec.end() ? (T *)nullptr : Vec.begin()),
        Length(Vec.size()) {}
DIAGNOSTIC_POP()

  /// Construct an ArrayRef<const T*> from ArrayRef<T*>. This uses SFINAE to
  /// ensure that only ArrayRefs of pointers can be converted.
  template <typename U>
  ArrayRef(const ArrayRef<U *> &A,
           std::enable_if_t<std::is_convertible<U *const *, T const *>::value>
               * = nullptr)
      : Data(A.data()), Length(A.size()) {}

  /// Construct an ArrayRef<const T*> from Vec<T*>. This uses SFINAE
  /// to ensure that only vectors of pointers can be converted.
  template <typename U, typename A>
  ArrayRef(const Vec<U *, A> &Vec,
           std::enable_if_t<std::is_convertible<U *const *, T const *>::value>
               * = nullptr)
      : Data(Vec.data()), Length(Vec.size()) {}

  /// @}
  /// @name Simple Operations
  /// @{

  iterator begin() const { return Data; }
  iterator end() const { return Data + Length; }

  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }

  /// empty - Check if the array is empty.
  bool empty() const { return Length == 0; }

  const T *data() const { return Data; }

  /// size - Get the array size.
  usize size() const { return Length; }

  /// front - Get the first element.
  const T &front() const {
    assert(!empty());
    return Data[0];
  }

  /// back - Get the last element.
  const T &back() const {
    assert(!empty());
    return Data[Length-1];
  }

  // copy - Allocate copy in Allocator and return ArrayRef<T> to it.
  template <typename Allocator> MutArrayRef<T> copy(Allocator &A) {
    T *Buff = A.template Allocate<T>(Length);
    std::uninitialized_copy(begin(), end(), Buff);
    return MutArrayRef<T>(Buff, Length);
  }

  /// equals - Check for element-wise equality.
  bool equals(ArrayRef RHS) const {
    if (Length != RHS.Length)
      return false;
    return std::equal(begin(), end(), RHS.begin());
  }

  /// slice(n, m) - Chop off the first N elements of the array, and keep M
  /// elements in the array.
  ArrayRef<T> slice(usize N, usize M) const {
    assert(N+M <= size() && "Invalid specifier");
    return ArrayRef<T>(data()+N, M);
  }

  /// slice(n) - Chop off the first N elements of the array.
  ArrayRef<T> slice(usize N) const { return slice(N, size() - N); }

  /// Drop the first \p N elements of the array.
  ArrayRef<T> drop_front(usize N = 1) const {
    assert(size() >= N && "Dropping more elements than exist");
    return slice(N, size() - N);
  }

  /// Drop the last \p N elements of the array.
  ArrayRef<T> drop_back(usize N = 1) const {
    assert(size() >= N && "Dropping more elements than exist");
    return slice(0, size() - N);
  }

  /// Return a copy of *this with the first N elements satisfying the
  /// given predicate removed.
  template <class PredicateT> ArrayRef<T> drop_while(PredicateT Pred) const {
    return ArrayRef<T>(find_if_not(*this, Pred), end());
  }

  /// Return a copy of *this with the first N elements not satisfying
  /// the given predicate removed.
  template <class PredicateT> ArrayRef<T> drop_until(PredicateT Pred) const {
    return ArrayRef<T>(find_if(*this, Pred), end());
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

  /// Return the first N elements of this Array that satisfy the given
  /// predicate.
  template <class PredicateT> ArrayRef<T> take_while(PredicateT Pred) const {
    return ArrayRef<T>(begin(), find_if_not(*this, Pred));
  }

  /// Return the first N elements of this Array that don't satisfy the
  /// given predicate.
  template <class PredicateT> ArrayRef<T> take_until(PredicateT Pred) const {
    return ArrayRef<T>(begin(), find_if(*this, Pred));
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  const T &operator[](usize Index) const {
    assert(Index < Length && "Invalid index!");
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
  /// @name Expensive Operations
  /// @{
  Vec<T> vec() const {
    return Vec<T>(Data, Data+Length);
  }

  /// @}
  /// @name Conversion operators
  /// @{
  operator Vec<T>() const {
    return Vec<T>(Data, Data+Length);
  }

  /// @}
};

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
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = usize;
  using difference_type = ptrdiff_t;

  /// Construct an empty MutArrayRef.
  /*implicit*/ MutArrayRef() = default;

  /// Construct an empty MutArrayRef from std::nullopt.
  /*implicit*/ MutArrayRef(std::nullopt_t) : ArrayRef<T>() {}

  /// Construct a MutArrayRef from a single element.
  /*implicit*/ MutArrayRef(T &OneElt) : ArrayRef<T>(OneElt) {}

  /// Construct a MutArrayRef from a pointer and length.
  /*implicit*/ MutArrayRef(T *data, usize length)
    : ArrayRef<T>(data, length) {}

  /// Construct a MutArrayRef from a range.
  MutArrayRef(T *begin, T *end) : ArrayRef<T>(begin, end) {}

  /// Construct a MutArrayRef from a Vec.
  /*implicit*/ MutArrayRef(Vec<T> &Vec)
  : ArrayRef<T>(Vec) {}

  /// Construct a MutArrayRef from a std::span.
  template <usize Ext>
  /*implicit*/ constexpr MutArrayRef(std::span<T, Ext> Sp)
    : ArrayRef<T>(Sp) {}

  /// Construct a MutArrayRef from a std::array
  template <usize N>
  /*implicit*/ constexpr MutArrayRef(std::array<T, N> &Arr)
      : ArrayRef<T>(Arr) {}

  /// Construct a MutArrayRef from a C array.
  template <usize N>
  /*implicit*/ constexpr MutArrayRef(T (&Arr)[N]) : ArrayRef<T>(Arr) {}

  T *data() const { return const_cast<T*>(ArrayRef<T>::data()); }

  iterator begin() const { return data(); }
  iterator end() const { return data() + this->size(); }

  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }

  /// front - Get the first element.
  T &front() const {
    assert(!this->empty());
    return data()[0];
  }

  /// back - Get the last element.
  T &back() const {
    assert(!this->empty());
    return data()[this->size()-1];
  }

  /// slice(n, m) - Chop off the first N elements of the array, and keep M
  /// elements in the array.
  MutArrayRef<T> slice(usize N, usize M) const {
    assert(N + M <= this->size() && "Invalid specifier");
    return MutArrayRef<T>(this->data() + N, M);
  }

  /// slice(n) - Chop off the first N elements of the array.
  MutArrayRef<T> slice(usize N) const {
    return slice(N, this->size() - N);
  }

  /// Drop the first \p N elements of the array.
  MutArrayRef<T> drop_front(usize N = 1) const {
    assert(this->size() >= N && "Dropping more elements than exist");
    return slice(N, this->size() - N);
  }

  MutArrayRef<T> drop_back(usize N = 1) const {
    assert(this->size() >= N && "Dropping more elements than exist");
    return slice(0, this->size() - N);
  }

  /// Return a copy of *this with the first N elements satisfying the
  /// given predicate removed.
  template <class PredicateT>
  MutArrayRef<T> drop_while(PredicateT Pred) const {
    return MutArrayRef<T>(find_if_not(*this, Pred), end());
  }

  /// Return a copy of *this with the first N elements not satisfying
  /// the given predicate removed.
  template <class PredicateT>
  MutArrayRef<T> drop_until(PredicateT Pred) const {
    return MutArrayRef<T>(find_if(*this, Pred), end());
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

  /// Return the first N elements of this Array that satisfy the given
  /// predicate.
  template <class PredicateT>
  MutArrayRef<T> take_while(PredicateT Pred) const {
    return MutArrayRef<T>(begin(), find_if_not(*this, Pred));
  }

  /// Return the first N elements of this Array that don't satisfy the
  /// given predicate.
  template <class PredicateT>
  MutArrayRef<T> take_until(PredicateT Pred) const {
    return MutArrayRef<T>(begin(), find_if(*this, Pred));
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  T &operator[](usize Index) const {
    assert(Index < this->size() && "Invalid index!");
    return data()[Index];
  }
};

/// This is a MutArrayRef that owns its array.
template <typename T> class OwningArrayRef : public MutArrayRef<T> {
public:
  OwningArrayRef() = default;
  OwningArrayRef(usize Size) : MutArrayRef<T>(new T[Size], Size) {}

  OwningArrayRef(ArrayRef<T> Data)
      : MutArrayRef<T>(new T[Data.size()], Data.size()) {
    std::copy(Data.begin(), Data.end(), this->begin());
  }

  OwningArrayRef(OwningArrayRef &&Other) { *this = std::move(Other); }

  OwningArrayRef &operator=(OwningArrayRef &&Other) {
    delete[] this->data();
    this->MutArrayRef<T>::operator=(Other);
    Other.MutArrayRef<T>::operator=(MutArrayRef<T>());
    return *this;
  }

  ~OwningArrayRef() { delete[] this->data(); }
};

/// @name ArrayRef Deduction guides
/// @{
/// Deduction guide to construct an ArrayRef from a single element.
template <typename T> ArrayRef(const T &OneElt) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a pointer and length
template <typename T> ArrayRef(const T *data, usize length) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a range
template <typename T> ArrayRef(const T *data, const T *end) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a SmallVec
template <typename T> ArrayRef(const SmallVecImpl<T> &Vec) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a SmallVec
template <typename T, unsigned N>
ArrayRef(const SmallVec<T, N> &Vec) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a Vec
template <typename T> ArrayRef(const Vec<T> &Vec) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a std::span
template <typename T, usize Ext>
ArrayRef(std::span<T, Ext> Sp) -> ArrayRef<T>;

/// Deduction guide to construct an ArrayRef from a std::array
template <typename T, usize N>
ArrayRef(const std::array<T, N> &Vec) -> ArrayRef<T>;

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

/// Deduction guide to construct a `MutArrayRef` from a pointer and
/// length.
template <class T>
MutArrayRef(T *data, usize length) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutableArrayRef` from a `SmallVector`.
template <class T>
MutArrayRef(SmallVecImpl<T> &Vec) -> MutArrayRef<T>;

template <class T, unsigned N>
MutArrayRef(SmallVec<T, N> &Vec) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a `Vec`.
template <class T> MutArrayRef(Vec<T> &Vec) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a `std::span`.
template <typename T, usize Ext>
MutArrayRef(std::span<T, Ext> Sp) -> MutArrayRef<T>;

/// Deduction guide to construct a `MutArrayRef` from a `std::array`.
template <class T, usize N>
MutArrayRef(std::array<T, N> &Vec) -> MutArrayRef<T>;

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
inline bool operator==(SmallVecImpl<T> &LHS, ArrayRef<T> RHS) {
  return ArrayRef<T>(LHS).equals(RHS);
}

template <typename T>
inline bool operator!=(ArrayRef<T> LHS, ArrayRef<T> RHS) {
  return !(LHS == RHS);
}

template <typename T>
inline bool operator!=(SmallVecImpl<T> &LHS, ArrayRef<T> RHS) {
  return !(LHS == RHS);
}

/// @}

} // namespace exi
