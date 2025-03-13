//===- Common/PagedVec.hpp ------------------------------------------===//
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
///
/// \file
/// This file defines the PagedVec class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Option.hpp>
#include <Common/MaybeBox.hpp>
#include <Common/SmallVec.hpp>
#include <Common/Vec.hpp>
#include <Common/iterator_range.hpp>
#include <Support/Allocator.hpp>
#include <cassert>

namespace exi {

/// A vector that allocates memory in pages.
///
/// Order is kept, but memory is allocated only when one element of the page is
/// accessed. This introduces a level of indirection, but it is useful when you
/// have a sparsely initialised vector where the full size is allocated upfront.
///
/// As a side effect the elements are initialised later than in a normal vector.
/// On the first access to one of the elements of a given page, all the elements
/// of the page are initialised. This also means that the elements of the page
/// are initialised beyond the size of the vector.
///
/// Similarly on destruction the elements are destroyed only when the page is
/// not needed anymore, delaying invoking the destructor of the elements.
///
/// Notice that this has iterators only on materialized elements. This
/// is deliberately done under the assumption you would dereference the elements
/// while iterating, therefore materialising them and losing the gains in terms
/// of memory usage this container provides. If you have such a use case, you
/// probably want to use a normal std::vector or an exi::SmallVec.
template <typename T, usize PageSize = 1024 / sizeof(T)> class PagedVec {
  static_assert(PageSize > 1, "PageSize must be greater than 0. Most likely "
                              "you want it to be greater than 16.");
  /// The actual number of elements in the vector which can be accessed.
  usize Size = 0;

  /// The position of the initial element of the page in the Data vector.
  /// Pages are allocated contiguously in the Data vector.
  mutable SmallVec<T*, 0> PageToDataPtrs;
  /// Actual page data. All the page elements are allocated on the
  /// first access of any of the elements of the page. Elements are default
  /// constructed and elements of the page are stored contiguously.
  MaybeBox<BumpPtrAllocator> Allocator;

  static MaybeBox<BumpPtrAllocator>
   WrapOrMakeNew(Option<BumpPtrAllocator&> A) {
    if (A.has_value())
      return MaybeBox<BumpPtrAllocator>(&*A, false);
    return std::make_unique<BumpPtrAllocator>();
  }

public:
  using value_type = T;

  /// Default constructor. We build our own allocator and box it.
  PagedVec() : Allocator(new BumpPtrAllocator, true) {}
  /// Reference constructor. We wrap the passed allocator.
  explicit PagedVec(BumpPtrAllocator& A) : Allocator(&A, false) {}
  /// Pointer constructor. We wrap the passed allocator, or a box a new one.
  explicit PagedVec(BumpPtrAllocator* A) : Allocator(WrapOrMakeNew(A)) {}

  ~PagedVec() { clear(); }

  // Forbid copy and move as we do not need them for the current use case.
  PagedVec(const PagedVec&) = delete;
  PagedVec(PagedVec&&) = delete;
  PagedVec &operator=(const PagedVec&) = delete;
  PagedVec &operator=(PagedVec&&) = delete;

  /// Look up an element at position `Index`.
  /// If the associated page is not filled, it will be filled with default
  /// constructed elements.
  T& operator[](usize Index) const {
    assert(Index < Size);
    assert(Index / PageSize < PageToDataPtrs.size());
    T*& PagePtr = PageToDataPtrs[Index / PageSize];
    // If the page was not yet allocated, allocate it.
    if EXI_UNLIKELY(!PagePtr) {
      PagePtr = Allocator.getPointer()->template Allocate<T>(PageSize);
      // We need to invoke the default constructor on all the elements of the
      // page.
      std::uninitialized_value_construct_n(PagePtr, PageSize);
    }
    // Dereference the element in the page.
    return PagePtr[Index % PageSize];
  }

  /// Return the capacity of the vector. I.e. the maximum size it can be
  /// expanded to with the resize method without allocating more pages.
  [[nodiscard]] usize capacity() const {
    return PageToDataPtrs.size() * PageSize;
  }

  /// Return the size of the vector.
  [[nodiscard]] usize size() const { return Size; }

  /// Resize the vector. Notice that the constructor of the elements will not
  /// be invoked until an element of a given page is accessed, at which point
  /// all the elements of the page will be constructed.
  ///
  /// If the new size is smaller than the current size, the elements of the
  /// pages that are not needed anymore will be destroyed, however, elements of
  /// the last page will not be destroyed.
  ///
  /// For these reason the usage of this vector is discouraged if you rely
  /// on the construction / destructor of the elements to be invoked.
  void resize(usize NewSize) {
    if EXI_UNLIKELY(NewSize == 0) {
      clear();
      return;
    }
    // Handle shrink case: destroy the elements in the pages that are not
    // needed any more and deallocate the pages.
    //
    // On the other hand, we do not destroy the extra elements in the last page,
    // because we might need them later and the logic is simpler if we do not
    // destroy them. This means that elements are only destroyed when the
    // page they belong to is destroyed. This is similar to what happens on
    // access of the elements of a page, where all the elements of the page are
    // constructed not only the one effectively needed.
    usize NewLastPage = (NewSize - 1) / PageSize;
    if (NewSize < Size) {
      for (usize I = NewLastPage + 1, N = PageToDataPtrs.size(); I < N; ++I) {
        T *Page = PageToDataPtrs[I];
        if (!Page)
          continue;
        // We need to invoke the destructor on all the elements of the page.
        std::destroy_n(Page, PageSize);
        Allocator.getPointer()->Deallocate(Page);
      }
    }

    Size = NewSize;
    PageToDataPtrs.resize(NewLastPage + 1);
  }

  [[nodiscard]] bool empty() const { return Size == 0; }

  /// Clear the vector, i.e. clear the allocated pages, the whole page
  /// lookup index and reset the size.
  void clear() {
    Size = 0;
    for (T* Page : PageToDataPtrs) {
      if (Page == nullptr)
        continue;
      std::destroy_n(Page, PageSize);
      // If we do not own the allocator, deallocate the pages one by one.
      if (!Allocator.getInt())
        Allocator.getPointer()->Deallocate(Page);
    }
    // If we own the allocator, simply reset it.
    if (Allocator.getInt())
      Allocator.getPointer()->Reset();
    PageToDataPtrs.clear();
  }

  /// Iterator on all the elements of the vector
  /// which have actually being constructed.
  class MaterializedIterator {
    const PagedVec* PV;
    usize ElementIdx;

  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    MaterializedIterator(const PagedVec* PV, usize ElementIdx)
        : PV(PV), ElementIdx(ElementIdx) {}

    /// Pre-increment operator.
    ///
    /// When incrementing the iterator, we skip the elements which have not
    /// been materialized yet.
    MaterializedIterator& operator++() {
      ++ElementIdx;
      if (ElementIdx % PageSize == 0) {
        while (ElementIdx < PV->Size &&
               !PV->PageToDataPtrs[ElementIdx / PageSize])
          ElementIdx += PageSize;
        if (ElementIdx > PV->Size)
          ElementIdx = PV->Size;
      }

      return *this;
    }

    MaterializedIterator operator++(int) {
      MaterializedIterator Copy = *this;
      ++*this;
      return Copy;
    }

    const T& operator*() const {
      assert(ElementIdx < PV->Size);
      assert(PV->PageToDataPtrs[ElementIdx / PageSize]);
      T* PagePtr = PV->PageToDataPtrs[ElementIdx / PageSize];
      return PagePtr[ElementIdx % PageSize];
    }

    /// Equality operator.
    friend bool operator==(const MaterializedIterator& LHS,
                           const MaterializedIterator& RHS) {
      return LHS.equals(RHS);
    }

    [[nodiscard]] usize getIndex() const { return ElementIdx; }

  private:
    EXI_INLINE void verify() const {
      assert(
          ElementIdx == PV->Size ||
          (ElementIdx < PV->Size && PV->PageToDataPtrs[ElementIdx / PageSize]));
    }

    bool equals(const MaterializedIterator& Other) const {
      assert(PV == Other.PV);
      this->verify();
      Other.verify();
      return ElementIdx == Other.ElementIdx;
    }
  };

  /// Iterators over the materialized elements of the vector.
  ///
  /// This includes all the elements belonging to allocated pages,
  /// even if they have not been accessed yet. It's enough to access
  /// one element of a page to materialize all the elements of the page.
  MaterializedIterator materialized_begin() const {
    // Look for the first valid page.
    for (usize ElementIdx = 0; ElementIdx < Size; ElementIdx += PageSize)
      if (PageToDataPtrs[ElementIdx / PageSize])
        return MaterializedIterator(this, ElementIdx);

    return MaterializedIterator(this, Size);
  }

  MaterializedIterator materialized_end() const {
    return MaterializedIterator(this, Size);
  }

  [[nodiscard]] exi::iterator_range<MaterializedIterator> materialized() const {
    return {materialized_begin(), materialized_end()};
  }
};

} // namespace exi
