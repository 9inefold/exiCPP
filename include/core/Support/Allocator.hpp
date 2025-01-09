//===- Support/Allocator.hpp -----------------------------------------===//
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
/// \file
///
/// This file defines the BumpPtrAllocator interface. BumpPtrAllocator conforms
/// to the LLVM "Allocator" concept and is similar to MallocAllocator, but
/// objects cannot be deallocated. Their lifetime is tied to the lifetime of the
/// allocator.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/Option.hpp>
#include <Common/SmallVec.hpp>
#include <Support/Alignment.hpp>
#include <Support/AllocatorBase.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/MathExtras.hpp>
#include <algorithm>
#include <iterator>
#include <utility>

namespace exi {

namespace H {

// We call out to an external function to actually print the message as the
// printing code uses Allocator.hpp in its implementation.
void printBumpPtrAllocatorStats(unsigned NumSlabs, usize BytesAllocated,
                                usize TotalMemory);

} // end namespace H

/// Allocate memory in an ever growing pool, as if by bump-pointer.
///
/// This isn't strictly a bump-pointer allocator as it uses backing slabs of
/// memory rather than relying on a boundless contiguous heap. However, it has
/// bump-pointer semantics in that it is a monotonically growing pool of memory
/// where every allocation is found by merely allocating the next N bytes in
/// the slab, or the next N bytes in the next slab.
///
/// Note that this also has a threshold for forcing allocations above a certain
/// size into their own slab.
///
/// The BumpPtrAllocatorImpl template defaults to using a MallocAllocator
/// object, which wraps malloc, to allocate memory, but it can be changed to
/// use a custom allocator.
///
/// The GrowthDelay specifies after how many allocated slabs the allocator
/// increases the size of the slabs.
template <typename AllocatorT = MallocAllocator, usize SlabSize = 4096,
          usize SizeThreshold = SlabSize, usize GrowthDelay = 128>
class BumpPtrAllocatorImpl
    : public AllocatorBase<BumpPtrAllocatorImpl<AllocatorT, SlabSize,
                                                SizeThreshold, GrowthDelay>>,
      private H::AllocatorHolder<AllocatorT> {
  using AllocTy = H::AllocatorHolder<AllocatorT>;

public:
  static_assert(SizeThreshold <= SlabSize,
                "The SizeThreshold must be at most the SlabSize to ensure "
                "that objects larger than a slab go into their own memory "
                "allocation.");
  static_assert(GrowthDelay > 0,
                "GrowthDelay must be at least 1 which already increases the"
                "slab size after each allocated slab.");

  BumpPtrAllocatorImpl() = default;

  template <typename T>
  BumpPtrAllocatorImpl(T &&Allocator)
      : AllocTy(std::forward<T &&>(Allocator)) {}

  // Manually implement a move constructor as we must clear the old allocator's
  // slabs as a matter of correctness.
  BumpPtrAllocatorImpl(BumpPtrAllocatorImpl &&Old)
      : AllocTy(std::move(Old.getAllocator())), CurPtr(Old.CurPtr),
        End(Old.End), Slabs(std::move(Old.Slabs)),
        CustomSizedSlabs(std::move(Old.CustomSizedSlabs)),
        BytesAllocated(Old.BytesAllocated), RedZoneSize(Old.RedZoneSize) {
    Old.CurPtr = Old.End = nullptr;
    Old.BytesAllocated = 0;
    Old.Slabs.clear();
    Old.CustomSizedSlabs.clear();
  }

  ~BumpPtrAllocatorImpl() {
    DeallocateSlabs(Slabs.begin(), Slabs.end());
    DeallocateCustomSizedSlabs();
  }

  BumpPtrAllocatorImpl &operator=(BumpPtrAllocatorImpl &&RHS) {
    DeallocateSlabs(Slabs.begin(), Slabs.end());
    DeallocateCustomSizedSlabs();

    CurPtr = RHS.CurPtr;
    End = RHS.End;
    BytesAllocated = RHS.BytesAllocated;
    RedZoneSize = RHS.RedZoneSize;
    Slabs = std::move(RHS.Slabs);
    CustomSizedSlabs = std::move(RHS.CustomSizedSlabs);
    AllocTy::operator=(std::move(RHS.getAllocator()));

    RHS.CurPtr = RHS.End = nullptr;
    RHS.BytesAllocated = 0;
    RHS.Slabs.clear();
    RHS.CustomSizedSlabs.clear();
    return *this;
  }

  /// Deallocate all but the current slab and reset the current pointer
  /// to the beginning of it, freeing all memory allocated so far.
  void Reset() {
    // Deallocate all but the first slab, and deallocate all custom-sized slabs.
    DeallocateCustomSizedSlabs();
    CustomSizedSlabs.clear();

    if (Slabs.empty())
      return;

    // Reset the state.
    BytesAllocated = 0;
    CurPtr = (char *)Slabs.front();
    End = CurPtr + SlabSize;

    __asan_poison_memory_region(*Slabs.begin(), computeSlabSize(0));
    DeallocateSlabs(std::next(Slabs.begin()), Slabs.end());
    Slabs.erase(std::next(Slabs.begin()), Slabs.end());
  }

  /// Allocate space at the specified alignment.
  // This method is *not* marked noalias, because
  // SpecificBumpPtrAllocator::DestroyAll() loops over all allocations, and
  // that loop is not based on the Allocate() return value.
  //
  // Allocate(0, N) is valid, it returns a non-null pointer (which should not
  // be dereferenced).
  EXI_RETURNS_NONNULL void *Allocate(usize Size, Align Alignment) {
    // Keep track of how many bytes we've allocated.
    BytesAllocated += Size;

    uptr AlignedPtr = alignAddr(CurPtr, Alignment);

    usize SizeToAllocate = Size;
#if EXI_ADDRESS_SANITIZER_BUILD
    // Add trailing bytes as a "red zone" under ASan.
    SizeToAllocate += RedZoneSize;
#endif

    uptr AllocEndPtr = AlignedPtr + SizeToAllocate;
    exi_assert(AllocEndPtr >= uptr(CurPtr),
           "Alignment + Size must not overflow");

    // Check if we have enough space.
    if (EXI_LIKELY(AllocEndPtr <= uptr(End)
                    // We can't return nullptr even for a zero-sized allocation!
                    && CurPtr != nullptr)) {
      CurPtr = reinterpret_cast<char *>(AllocEndPtr);
      // Update the allocation point of this memory block in MemorySanitizer.
      // Without this, MemorySanitizer messages for values originated from here
      // will point to the allocation of the entire slab.
      __msan_allocated_memory(reinterpret_cast<char *>(AlignedPtr), Size);
      // Similarly, tell ASan about this space.
      __asan_unpoison_memory_region(reinterpret_cast<char *>(AlignedPtr), Size);
      return reinterpret_cast<char *>(AlignedPtr);
    }

    return AllocateSlow(Size, SizeToAllocate, Alignment);
  }

  EXI_RETURNS_NONNULL EXI_NO_INLINE void *
  AllocateSlow(usize Size, usize SizeToAllocate, Align Alignment) {
    // If Size is really big, allocate a separate slab for it.
    usize PaddedSize = SizeToAllocate + Alignment.value() - 1;
    if (PaddedSize > SizeThreshold) {
      void *NewSlab =
          this->getAllocator().Allocate(PaddedSize, alignof(std::max_align_t));
      // We own the new slab and don't want anyone reading anyting other than
      // pieces returned from this method.  So poison the whole slab.
      __asan_poison_memory_region(NewSlab, PaddedSize);
      CustomSizedSlabs.push_back(std::make_pair(NewSlab, PaddedSize));

      uptr AlignedAddr = alignAddr(NewSlab, Alignment);
      assert(AlignedAddr + Size <= uptr(NewSlab) + PaddedSize);
      char *AlignedPtr = (char*)AlignedAddr;
      __msan_allocated_memory(AlignedPtr, Size);
      __asan_unpoison_memory_region(AlignedPtr, Size);
      return AlignedPtr;
    }

    // Otherwise, start a new slab and try again.
    StartNewSlab();
    uptr AlignedAddr = alignAddr(CurPtr, Alignment);
    exi_assert(AlignedAddr + SizeToAllocate <= uptr(End),
           "Unable to allocate memory!");
    char *AlignedPtr = (char*)AlignedAddr;
    CurPtr = AlignedPtr + SizeToAllocate;
    __msan_allocated_memory(AlignedPtr, Size);
    __asan_unpoison_memory_region(AlignedPtr, Size);
    return AlignedPtr;
  }

  inline EXI_RETURNS_NONNULL void *
  Allocate(usize Size, usize Alignment) {
    exi_assert(Alignment > 0, "0-byte alignment is not allowed. Use 1 instead.");
    return Allocate(Size, Align(Alignment));
  }

  // Pull in base class overloads.
  using AllocatorBase<BumpPtrAllocatorImpl>::Allocate;

  // Bump pointer allocators are expected to never free their storage; and
  // clients expect pointers to remain valid for non-dereferencing uses even
  // after deallocation.
  void Deallocate(const void *Ptr, usize Size, usize /*Alignment*/) {
    __asan_poison_memory_region(Ptr, Size);
  }

  // Pull in base class overloads.
  using AllocatorBase<BumpPtrAllocatorImpl>::Deallocate;

  usize GetNumSlabs() const { return Slabs.size() + CustomSizedSlabs.size(); }

  /// \return An index uniquely and reproducibly identifying
  /// an input pointer \p Ptr in the given allocator.
  /// The returned value is negative iff the object is inside a custom-size
  /// slab.
  /// Returns an empty optional if the pointer is not found in the allocator.
  Option<i64> identifyObject(const void *Ptr) {
    const char *P = static_cast<const char *>(Ptr);
    i64 InSlabIdx = 0;
    for (usize Idx = 0, E = Slabs.size(); Idx < E; Idx++) {
      const char *S = static_cast<const char *>(Slabs[Idx]);
      if (P >= S && P < S + computeSlabSize(Idx))
        return InSlabIdx + static_cast<i64>(P - S);
      InSlabIdx += static_cast<i64>(computeSlabSize(Idx));
    }

    // Use negative index to denote custom sized slabs.
    i64 InCustomSizedSlabIdx = -1;
    for (usize Idx = 0, E = CustomSizedSlabs.size(); Idx < E; Idx++) {
      const char *S = static_cast<const char *>(CustomSizedSlabs[Idx].first);
      usize Size = CustomSizedSlabs[Idx].second;
      if (P >= S && P < S + Size)
        return InCustomSizedSlabIdx - static_cast<i64>(P - S);
      InCustomSizedSlabIdx -= static_cast<i64>(Size);
    }
    return std::nullopt;
  }

  /// A wrapper around identifyObject that additionally asserts that
  /// the object is indeed within the allocator.
  /// \return An index uniquely and reproducibly identifying
  /// an input pointer \p Ptr in the given allocator.
  i64 identifyKnownObject(const void *Ptr) {
    Option<i64> Out = identifyObject(Ptr);
    exi_assert(Out, "Wrong allocator used");
    return *Out;
  }

  /// A wrapper around identifyKnownObject. Accepts type information
  /// about the object and produces a smaller identifier by relying on
  /// the alignment information. Note that sub-classes may have different
  /// alignment, so the most base class should be passed as template parameter
  /// in order to obtain correct results. For that reason automatic template
  /// parameter deduction is disabled.
  /// \return An index uniquely and reproducibly identifying
  /// an input pointer \p Ptr in the given allocator. This identifier is
  /// different from the ones produced by identifyObject and
  /// identifyAlignedObject.
  template <typename T>
  i64 identifyKnownAlignedObject(const void *Ptr) {
    i64 Out = identifyKnownObject(Ptr);
    exi_assert(Out % alignof(T) == 0, "Wrong alignment information");
    return Out / alignof(T);
  }

  usize getTotalMemory() const {
    usize TotalMemory = 0;
    for (auto I = Slabs.begin(), E = Slabs.end(); I != E; ++I)
      TotalMemory += computeSlabSize(std::distance(Slabs.begin(), I));
    for (const auto &PtrAndSize : CustomSizedSlabs)
      TotalMemory += PtrAndSize.second;
    return TotalMemory;
  }

  usize getBytesAllocated() const { return BytesAllocated; }

  void setRedZoneSize(usize NewSize) {
    RedZoneSize = NewSize;
  }

  void PrintStats() const {
    H::printBumpPtrAllocatorStats(Slabs.size(), BytesAllocated,
                                       getTotalMemory());
  }

private:
  /// The current pointer into the current slab.
  ///
  /// This points to the next free byte in the slab.
  char *CurPtr = nullptr;

  /// The end of the current slab.
  char *End = nullptr;

  /// The slabs allocated so far.
  SmallVec<void *, 4> Slabs;

  /// Custom-sized slabs allocated for too-large allocation requests.
  SmallVec<std::pair<void *, usize>, 0> CustomSizedSlabs;

  /// How many bytes we've allocated.
  ///
  /// Used so that we can compute how much space was wasted.
  usize BytesAllocated = 0;

  /// The number of bytes to put between allocations when running under
  /// a sanitizer.
  usize RedZoneSize = 1;

  static usize computeSlabSize(unsigned SlabIdx) {
    // Scale the actual allocated slab size based on the number of slabs
    // allocated. Every GrowthDelay slabs allocated, we double
    // the allocated size to reduce allocation frequency, but saturate at
    // multiplying the slab size by 2^30.
    return SlabSize *
           ((usize)1 << std::min<usize>(30, SlabIdx / GrowthDelay));
  }

  /// Allocate a new slab and move the bump pointers over into the new
  /// slab, modifying CurPtr and End.
  void StartNewSlab() {
    usize AllocatedSlabSize = computeSlabSize(Slabs.size());

    void *NewSlab = this->getAllocator().Allocate(AllocatedSlabSize,
                                                  alignof(std::max_align_t));
    // We own the new slab and don't want anyone reading anything other than
    // pieces returned from this method.  So poison the whole slab.
    __asan_poison_memory_region(NewSlab, AllocatedSlabSize);

    Slabs.push_back(NewSlab);
    CurPtr = (char *)(NewSlab);
    End = ((char *)NewSlab) + AllocatedSlabSize;
  }

  /// Deallocate a sequence of slabs.
  void DeallocateSlabs(SmallVecImpl<void *>::iterator I,
                       SmallVecImpl<void *>::iterator E) {
    for (; I != E; ++I) {
      usize AllocatedSlabSize =
          computeSlabSize(std::distance(Slabs.begin(), I));
      this->getAllocator().Deallocate(*I, AllocatedSlabSize,
                                      alignof(std::max_align_t));
    }
  }

  /// Deallocate all memory for custom sized slabs.
  void DeallocateCustomSizedSlabs() {
    for (auto &PtrAndSize : CustomSizedSlabs) {
      void *Ptr = PtrAndSize.first;
      usize Size = PtrAndSize.second;
      this->getAllocator().Deallocate(Ptr, Size, alignof(std::max_align_t));
    }
  }

  template <typename T> friend class SpecificBumpPtrAllocator;
};

/// The standard BumpPtrAllocator which just uses the default template
/// parameters.
typedef BumpPtrAllocatorImpl<> BumpPtrAllocator;

/// A BumpPtrAllocator that allows only elements of a specific type to be
/// allocated.
///
/// This allows calling the destructor in DestroyAll() and when the allocator is
/// destroyed.
template <typename T> class SpecificBumpPtrAllocator {
  BumpPtrAllocator Allocator;

public:
  SpecificBumpPtrAllocator() {
    // Because SpecificBumpPtrAllocator walks the memory to call destructors,
    // it can't have red zones between allocations.
    Allocator.setRedZoneSize(0);
  }
  SpecificBumpPtrAllocator(SpecificBumpPtrAllocator &&Old)
      : Allocator(std::move(Old.Allocator)) {}
  ~SpecificBumpPtrAllocator() { DestroyAll(); }

  SpecificBumpPtrAllocator &operator=(SpecificBumpPtrAllocator &&RHS) {
    Allocator = std::move(RHS.Allocator);
    return *this;
  }

  /// Call the destructor of each allocated object and deallocate all but the
  /// current slab and reset the current pointer to the beginning of it, freeing
  /// all memory allocated so far.
  void DestroyAll() {
    auto DestroyElements = [](char *Begin, char *End) {
      assert(Begin == (char *)alignAddr(Begin, Align::Of<T>()));
      for (char *Ptr = Begin; Ptr + sizeof(T) <= End; Ptr += sizeof(T))
        reinterpret_cast<T *>(Ptr)->~T();
    };

    for (auto I = Allocator.Slabs.begin(), E = Allocator.Slabs.end(); I != E;
         ++I) {
      usize AllocatedSlabSize = BumpPtrAllocator::computeSlabSize(
          std::distance(Allocator.Slabs.begin(), I));
      char *Begin = (char *)alignAddr(*I, Align::Of<T>());
      char *End = *I == Allocator.Slabs.back() ? Allocator.CurPtr
                                               : (char *)*I + AllocatedSlabSize;

      DestroyElements(Begin, End);
    }

    for (auto &PtrAndSize : Allocator.CustomSizedSlabs) {
      void *Ptr = PtrAndSize.first;
      usize Size = PtrAndSize.second;
      DestroyElements((char *)alignAddr(Ptr, Align::Of<T>()),
                      (char *)Ptr + Size);
    }

    Allocator.Reset();
  }

  /// Allocate space for an array of objects without constructing them.
  T *Allocate(usize num = 1) { return Allocator.Allocate<T>(num); }

  /// \return An index uniquely and reproducibly identifying
  /// an input pointer \p Ptr in the given allocator.
  /// Returns an empty optional if the pointer is not found in the allocator.
  Option<i64> identifyObject(const void *Ptr) {
    return Allocator.identifyObject(Ptr);
  }
};

} // namespace exi

template <typename AllocatorT, usize SlabSize, usize SizeThreshold,
          usize GrowthDelay>
void *
operator new(usize Size,
             exi::BumpPtrAllocatorImpl<AllocatorT, SlabSize, SizeThreshold,
                                        GrowthDelay> &Allocator) {
  return Allocator.Allocate(Size, std::min((usize)exi::NextPowerOf2(Size),
                                           alignof(std::max_align_t)));
}

template <typename AllocatorT, usize SlabSize, usize SizeThreshold,
          usize GrowthDelay>
void operator delete(void *,
                     exi::BumpPtrAllocatorImpl<AllocatorT, SlabSize,
                                                SizeThreshold, GrowthDelay> &) {
}
