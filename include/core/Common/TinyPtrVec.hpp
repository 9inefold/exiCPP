//===- Common/TinyPtrVec.hpp ----------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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

#include <Common/ArrayRef.hpp>
#include <Common/PointerUnion.hpp>
#include <Common/SmallVec.hpp>
#include <Support/ErrorHandle.hpp>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace exi {

/// TinyPtrVec - This class is specialized for cases where there are
/// normally 0 or 1 element in a vector, but is general enough to go beyond that
/// when required.
///
/// NOTE: This container doesn't allow you to store a null pointer into it.
///
template <typename T>
class TinyPtrVec {
public:
  using VecT = SmallVec<T, 4>;
  using value_type = typename VecT::value_type;
  // T must be the first pointer type so that is<T> is true for the
  // default-constructed PtrUnion. This allows an empty TinyPtrVec to
  // naturally vend a begin/end iterator of type T* without an additional
  // check for the empty state.
  using PtrUnion = PointerUnion<T, VecT*>;

private:
  PtrUnion Data;

public:
  TinyPtrVec() = default;

  ~TinyPtrVec() {
    if (VecT* V = dyn_cast_if_present<VecT*>(Data))
      delete V;
  }

  TinyPtrVec(const TinyPtrVec& RHS) : Data(RHS.Data) {
    if (VecT* V = dyn_cast_if_present<VecT*>(Data))
      Data = new VecT(*V);
  }

  TinyPtrVec& operator=(const TinyPtrVec& RHS) {
    if (this == &RHS)
      return *this;
    if (RHS.empty()) {
      this->clear();
      return *this;
    }

    // Try to squeeze into the single slot. If it won't fit, allocate a copied
    // vector.
    if (isa<T>(Data)) {
      if (RHS.size() == 1)
        Data = RHS.front();
      else
        Data = new VecT(*cast<VecT*>(RHS.Data));
      return *this;
    }

    // If we have a full vector allocated, try to re-use it.
    if (isa<T>(RHS.Data)) {
      cast<VecT*>(Data)->clear();
      cast<VecT*>(Data)->push_back(RHS.front());
    } else {
      *cast<VecT*>(Data) = *cast<VecT*>(RHS.Data);
    }
    return *this;
  }

  TinyPtrVec(TinyPtrVec&& RHS) : Data(RHS.Data) {
    RHS.Data = (T)nullptr;
  }

  TinyPtrVec& operator=(TinyPtrVec&& RHS) {
    if (this == &RHS)
      return *this;
    if (RHS.empty()) {
      this->clear();
      return *this;
    }

    // If this vector has been allocated on the heap, re-use it if cheap. If it
    // would require more copying, just delete it and we'll steal the other
    // side.
    if (VecT* V = dyn_cast_if_present<VecT*>(Data)) {
      if (isa<T>(RHS.Data)) {
        V->clear();
        V->push_back(RHS.front());
        RHS.Data = T();
        return *this;
      }
      delete V;
    }

    Data = RHS.Data;
    RHS.Data = T();
    return *this;
  }

  TinyPtrVec(std::initializer_list<T> IL)
      : Data(IL.size() == 0
                ? PtrUnion()
                : IL.size() == 1 ? PtrUnion(*IL.begin())
                                 : PtrUnion(new VecT(IL.begin(), IL.end()))) {}

  /// Constructor from an ArrayRef.
  ///
  /// This also is a constructor for individual array elements due to the single
  /// element constructor for ArrayRef.
  explicit TinyPtrVec(ArrayRef<T> Elts)
      : Data(Elts.empty()
                ? PtrUnion()
                : Elts.size() == 1
                      ? PtrUnion(Elts[0])
                      : PtrUnion(new VecT(Elts.begin(), Elts.end()))) {}

  TinyPtrVec(size_t Count, T Value)
      : Data(Count == 0 ? PtrUnion()
                       : Count == 1 ? PtrUnion(Value)
                                    : PtrUnion(new VecT(Count, Value))) {}

  // implicit conversion operator to ArrayRef.
  operator ArrayRef<T>() const {
    if (Data.isNull())
      return {};
    if (isa<T>(Data))
      return *Data.getAddrOfPtr1();
    return *cast<VecT*>(Data);
  }

  // implicit conversion operator to MutArrayRef.
  operator MutArrayRef<T>() {
    if (Data.isNull())
      return {};
    if (isa<T>(Data))
      return *Data.getAddrOfPtr1();
    return *cast<VecT*>(Data);
  }

  // Implicit conversion to ArrayRef<U> if T* implicitly converts to U*.
  template <
      typename U,
      std::enable_if_t<std::is_convertible<ArrayRef<T>, ArrayRef<U>>::value,
                       bool> = false>
  operator ArrayRef<U>() const {
    return operator ArrayRef<T>();
  }

  bool empty() const {
    // This vector can be empty if it contains no element, or if it
    // contains a pointer to an empty vector.
    if (Data.isNull()) return true;
    if (VecT* Vec = dyn_cast_if_present<VecT*>(Data))
      return Vec->empty();
    return false;
  }

  unsigned size() const {
    if (empty())
      return 0;
    if (isa<T>(Data))
      return 1;
    return cast<VecT*>(Data)->size();
  }

  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() {
    if (isa<T>(Data))
      return Data.getAddrOfPtr1();

    return cast<VecT*>(Data)->begin();
  }

  iterator end() {
    if (isa<T>(Data))
      return begin() + (Data.isNull() ? 0 : 1);

    return cast<VecT*>(Data)->end();
  }

  const_iterator begin() const {
    return (const_iterator)const_cast<TinyPtrVec*>(this)->begin();
  }

  const_iterator end() const {
    return (const_iterator)const_cast<TinyPtrVec*>(this)->end();
  }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  T operator[](unsigned Ix) const {
    exi_invariant(!Data.isNull(), "can't index into an empty vector");
    if (isa<T>(Data)) {
      exi_invariant(Ix == 0, "tinyvector index out of range");
      return cast<T>(Data);
    }

    exi_invariant(Ix < cast<VecT*>(Data)->size(),
									"tinyvector index out of range");
    return (*cast<VecT*>(Data))[Ix];
  }

  T front() const {
    exi_assert(!empty(), "vector empty");
    if (isa<T>(Data))
      return cast<T>(Data);
    return cast<VecT*>(Data)->front();
  }

  T back() const {
    exi_assert(!empty(), "vector empty");
    if (isa<T>(Data))
      return cast<T>(Data);
    return cast<VecT*>(Data)->back();
  }

  void push_back(T NewVal) {
    // If we have nothing, add something.
    if (Data.isNull()) {
      Data = NewVal;
      exi_invariant(!Data.isNull(), "Can't add a null value");
      return;
    }

    // If we have a single value, convert to a vector.
    if (isa<T>(Data)) {
      T V = cast<T>(Data);
      Data = new VecT();
      cast<VecT*>(Data)->push_back(V);
    }

    // Add the new value, we know we have a vector.
    cast<VecT*>(Data)->push_back(NewVal);
  }

  void pop_back() {
    // If we have a single value, convert to empty.
    if (isa<T>(Data))
      Data = (T)nullptr;
    else if (VecT* Vec = cast<VecT*>(Data))
      Vec->pop_back();
  }

  void reserve(unsigned NewSize) {
    if EXI_UNLIKELY(NewSize == 0)
      return;
    
    if (!isa<VecT*>(Data)) {
      if (NewSize == 1)
        return;
      // If we have a single value, convert to a vector.
      if (isa<T>(Data)) {
        T V = cast<T>(Data);
        Data = new VecT();
        cast<VecT*>(Data)->push_back(V);
      } else {
        Data = new VecT();
      }
    }

    // Add the new value, we know we have a vector.
    cast<VecT*>(Data)->reserve(NewSize);
  }

  void clear() {
    // If we have a single value, convert to empty.
    if (isa<T>(Data)) {
      Data = T();
    } else if (VecT* Vec = dyn_cast_if_present<VecT*>(Data)) {
      // If we have a vector form, just clear it.
      Vec->clear();
    }
    // Otherwise, we're already empty.
  }

  iterator erase(iterator I) {
    exi_assert(I >= begin(), "Iterator to erase is out of bounds.");
    exi_assert(I < end(), "Erasing at past-the-end iterator.");

    // If we have a single value, convert to empty.
    if (isa<T>(Data)) {
      if (I == begin())
        Data = T();
    } else if (VecT* Vec = dyn_cast_if_present<VecT*>(Data)) {
      // multiple items in a vector; just do the erase, there is no
      // benefit to collapsing back to a pointer
      return Vec->erase(I);
    }
    return end();
  }

  iterator erase(iterator S, iterator E) {
    exi_assert(S >= begin(), "Range to erase is out of bounds.");
    exi_assert(S <= E, "Trying to erase invalid range.");
    exi_assert(E <= end(), "Trying to erase past the end.");

    if (isa<T>(Data)) {
      if (S == begin() && S != E)
        Data = T();
    } else if (VecT* Vec = dyn_cast_if_present<VecT*>(Data)) {
      return Vec->erase(S, E);
    }
    return end();
  }

  iterator insert(iterator I, const T &Elt) {
    exi_assert(I >= this->begin(), "Insertion iterator is out of bounds.");
    exi_assert(I <= this->end(), "Inserting past the end of the vector.");
    if (I == end()) {
      push_back(Elt);
      return std::prev(end());
    }
    exi_assert(!Data.isNull(), "Null value with non-end insert iterator.");
    if (isa<T>(Data)) {
      T V = cast<T>(Data);
      exi_invariant(I == begin());
      Data = Elt;
      push_back(V);
      return begin();
    }

    return cast<VecT*>(Data)->insert(I, Elt);
  }

  template <typename ItTy>
  iterator insert(iterator I, ItTy From, ItTy To) {
    exi_assert(I >= this->begin(), "Insertion iterator is out of bounds.");
    exi_assert(I <= this->end(), "Inserting past the end of the vector.");
    if (From == To)
      return I;

    // If we have a single value, convert to a vector.
    ptrdiff_t Offset = I - begin();
    if (Data.isNull()) {
      if (std::next(From) == To) {
        Data = *From;
        return begin();
      }

      Data = new VecT();
    } else if (isa<T>(Data)) {
      T V = cast<T>(Data);
      Data = new VecT();
      cast<VecT*>(Data)->push_back(V);
    }
    return cast<VecT*>(Data)->insert(begin() + Offset, From, To);
  }
};

} // namespace exi
