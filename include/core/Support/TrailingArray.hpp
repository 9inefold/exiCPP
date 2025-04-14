//===- Support/TrailingArray.hpp ------------------------------------===//
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
/// This file defines an inline array that follows a class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/Fundamental.hpp>
#include <Common/STLExtras.hpp>
#include <Support/Alignment.hpp>
#include <Support/ErrorHandle.hpp>
#include <memory>

namespace exi {

/// TrailingArray offers the feature trailing array in the derived class.
/// Memory is allocated with the following layout:
///
/// 	[ Derived ][ TrailingArray ]
///
/// This allows for more convenient inline arrays with complex data.
template <class Derived, typename T>
class TrailingArray {
  unsigned Size = 0;

  ALWAYS_INLINE const T* getData() const {
    auto* const Super = static_cast<const Derived*>(this);
    auto* const Data = reinterpret_cast<const char*>(Super) + ArrayOffset();
    return reinterpret_cast<const T*>(Data);
  }

  ALWAYS_INLINE T* getData() {
    const TrailingArray* const thiz = this; 
    return const_cast<T*>(thiz->getData());
  }

protected:
  explicit TrailingArray(unsigned Size) : Size(Size) {
    static_assert(std::is_final_v<Derived>,
      "Derived is required to be final to avoid array overlap.");
    std::uninitialized_default_construct_n(getData(), Size);
  }

  template <typename IterTy>
  TrailingArray(unsigned Size, IterTy I, IterTy E) : Size(Size) {
    static_assert(std::is_final_v<Derived>,
      "Derived is required to be final to avoid array overlap.");
    exi_assert(std::distance(I, E) == Size, "Invalid TrailingArray input.");
    if constexpr (H::sort_trivially_copyable<IterTy>)
      std::memcpy(this->data(), I, Size * sizeof(T));
    else	
      std::uninitialized_copy(I, E, this->begin());
  }

  TrailingArray(const TrailingArray&) = delete;
  TrailingArray& operator=(const TrailingArray&) = delete;

  static constexpr unsigned ArrayOffset() noexcept {
    constexpr Align A(alignof(T));
    return alignTo(sizeof(Derived), A);
  }

  static constexpr unsigned NewSize(unsigned N) noexcept {
    constexpr unsigned Head = ArrayOffset();
    return Head + (N * sizeof(T));
  }

  [[nodiscard]] static Derived* New(unsigned N) {
    void* const Ptr = ::operator new(TrailingArray::NewSize(N));
    // TODO: Check alignment?
    return static_cast<Derived*>(Ptr);
  }

  ~TrailingArray() {
    std::destroy_n(this->data(), Size);
  }

public:
  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = unsigned;
  using difference_type = ptrdiff_t;

  pointer data() EXI_LIFETIMEBOUND { return getData(); }
  const_pointer data() const EXI_LIFETIMEBOUND { return getData(); }

  reference at(unsigned Ix) EXI_LIFETIMEBOUND {
    exi_invariant(Ix < size());
    return data()[Ix];
  }

  const_reference at(unsigned Ix) const EXI_LIFETIMEBOUND {
    exi_invariant(Ix < size());
    return data()[Ix];
  }

  EXI_INLINE reference
   operator[](unsigned Ix) EXI_LIFETIMEBOUND {
    return this->at(Ix);
  }

  EXI_INLINE const_reference
   operator[](unsigned Ix) const EXI_LIFETIMEBOUND {
    return this->at(Ix);
  }

  unsigned size() const { return Size; }
  bool empty() const { return Size == 0; }
  unsigned sizeInBytes() const { return size() * sizeof(T); }

  iterator begin() EXI_LIFETIMEBOUND { return getData(); }
  const_iterator begin() const EXI_LIFETIMEBOUND { return getData(); }
  iterator end() EXI_LIFETIMEBOUND { return begin() + Size; }
  const_iterator end() const EXI_LIFETIMEBOUND { return begin() + Size; }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return reverse_iterator(begin()); }
};

} // namespace exi
