//===- Patches.cpp --------------------------------------------------===//
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

#include <Patches.hpp>
#include <Fundamental.hpp>
#include <Mem.hpp>
#include <Strings.hpp>
#include <utility>

#ifdef _MSC_VER
# define STUB_ATTRS __noinline
# define RTL_REALLOC_ATTRS  \
  __declspec(allocator)     \
  __declspec(noalias)       \
  __declspec(restrict)
#elif __GNUC__
# define STUB_ATTRS __attribute__((__noinline__, used))
# define RTL_REALLOC_ATTRS
#endif

#define VCAST(...) static_cast<decltype(__VA_ARGS__)>((__VA_ARGS__))

using namespace re;

static_assert(sizeof(void*) == sizeof(void(*)(void)));
static_assert(sizeof(PatchData) == 0x50);
static_assert(sizeof(PerFuncPatchData) == 0x178);

namespace {
enum InternalStubKind {
  mi_usable_size,

  _expand_base,
  _realloc_base,
  _recalloc_base,
  _msize_base,

  _aligned_realloc,
  _aligned_free,
  _aligned_recalloc,
  _aligned_msize,
  _aligned_offset_realloc,
  _aligned_offset_recalloc,

  _malloc_dbg,
  _realloc_dbg,
  _calloc_dbg,
  _free_dbg,
  _expand_dbg,
  _recalloc_dbg,
  _msize_dbg,
  _aligned_malloc_dbg,
  _aligned_realloc_dbg,
  _aligned_recalloc_dbg,
  _aligned_offset_malloc_dbg,
  _aligned_offset_realloc_dbg,
  _aligned_offset_recalloc_dbg
};

STUB_ATTRS static void* Identity(volatile void* Ptr) {
  return Ptr;
}

STUB_ATTRS static usize Identity(volatile usize Val) {
  return Val;
}

ALWAYS_INLINE static usize Ret0Template() {
  volatile usize A = Identity(1);
  Identity(A);
  return 0; 
}

STUB_ATTRS static usize Returns0(usize) {
  volatile usize A = Identity(1);
  volatile usize B = Identity(A);
  return (B & ~usize(0b11)); 
}

//======================================================================//
// Stubs
//======================================================================//

template <usize I> using IdentTy = volatile usize;
template <bool B>  using VoidOr = std::conditional_t<B, void*, void>;

template <typename, bool B = true> struct StubHolder;

template <usize...II, bool B>
struct StubHolder<std::index_sequence<II...>, B> {
  using Void = VoidOr<B>;
  using FnTy = Void(void*, IdentTy<II>...);

  template <InternalStubKind K>
  STUB_ATTRS static void* Stub(void*, IdentTy<II>...) {
    void* Ptr = Identity(reinterpret_cast<void*>(1ull));
    Identity(Ptr);
    return nullptr;
  }

  template <InternalStubKind K>
  STUB_ATTRS static void* TermStub(void*, IdentTy<II>...) {
    void* Ptr = Identity(reinterpret_cast<void*>(1ull));
    Identity(Ptr);
    return nullptr;
  }

  template <InternalStubKind K>
  STUB_ATTRS static void* Me(volatile void* Ptr, IdentTy<II>...Args) {
    return Stub<K>(Ptr, Args...);
  }

  template <InternalStubKind K>
  STUB_ATTRS static void* MeTerm(volatile void* Ptr, IdentTy<II>...Args) {
    return TermStub<K>(Ptr, Args...);
  }
};

template <usize I, bool B = true>
using Stubs = StubHolder<
  std::make_index_sequence<re::min(I, 4ull) - 1>, B>;

template <usize I, bool B = true>
using stub_t = typename Stubs<I, B>::FnTy *;

template <InternalStubKind K, usize Count, bool B = true>
constexpr auto& Fn_ = Stubs<Count, B>::template Me<K>;

template <InternalStubKind K, usize Count, bool B = true>
constexpr auto& TermFn_ = Stubs<Count, B>::template MeTerm<K>;

template <InternalStubKind K, usize Count, bool B = true>
constexpr void* Fn = &Fn_<K, Count, B>;

template <InternalStubKind K, usize Count, bool B = true>
constexpr void* TermFn = &TermFn_<K, Count, B>;

//////////////////////////////////////////////////////////////////////////
// Subfunction Stubs

template <InternalStubKind K>
STUB_ATTRS static usize FnReturns0(void*) {
  return Ret0Template();
}

STUB_ATTRS static void* ReAlloc_recalloc(
 volatile void* Ptr, volatile usize Size) {
  volatile usize A = Identity(1);
  Identity(A);
  return nullptr;
}

} // namespace `anonymous`

//======================================================================//
// Rtl*Heap
//======================================================================//

static byte* _mi_RtlSizeHeap_RVA = nullptr;
static byte* _mi_RtlFreeHeap_RVA = nullptr;
static byte* _mi_RtlReAllocateHeap_RVA = nullptr;

STUB_ATTRS static u32 /*ULONG*/ __stdcall _mi_RtlSizeHeap(
 void* HeapHandle, u32 Flags, void* BaseAddress) {
  using SelfType = decltype(&_mi_RtlSizeHeap);
  if ((Returns0(usize(BaseAddress)) & 0xff) == 0) {
    const auto* Func = ptr_cast<SelfType>(_mi_RtlSizeHeap_RVA);
    if (Func == nullptr)
      return 0;
    return (*Func)(HeapHandle, Flags, BaseAddress);
  }

  return FnReturns0<mi_usable_size>(BaseAddress);
}

STUB_ATTRS static u32 /*LOGICAL*/ __stdcall _mi_RtlFreeHeap(
 void* HeapHandle, u32 Flags, void* BaseAddress) {
  using SelfType = decltype(&_mi_RtlFreeHeap);
  if ((Returns0(usize(BaseAddress)) & 0xff) == 0) {
    const auto* Func = ptr_cast<SelfType>(_mi_RtlFreeHeap_RVA);
    if (Func == nullptr)
      return false;
    return (*Func)(HeapHandle, Flags, BaseAddress);
  }

  return true;
}

RTL_REALLOC_ATTRS STUB_ATTRS static void* __stdcall _mi_RtlReAllocateHeap(
 void* HeapHandle, u32 Flags, void* BaseAddress, usize Size) {
  using SelfType = decltype(&_mi_RtlReAllocateHeap);
  Size &= 0xffffffff;
  if (!BaseAddress || (Returns0(usize(BaseAddress)) & 0xff) == 0) {
    // The default route, calls `RtlFreeHeap`.
    const auto* Func = ptr_cast<SelfType>(_mi_RtlFreeHeap_RVA);
    if (Func == nullptr)
      return nullptr;
    return (*Func)(HeapHandle, Flags, BaseAddress, Size);
  } else if ((Flags & 0x10/*HEAP_REALLOC_IN_PLACE_ONLY*/) == 0) {
    // The secondary route, calls `_recalloc_base`.
    if ((Flags & 0x8/*HEAP_ZERO_MEMORY*/) == 0) {
      return ReAlloc_recalloc(BaseAddress, Size);
    }
    return Stubs<2>::TermStub<_recalloc_base>(BaseAddress, Size);
  } else {
    // The final route, calls `_expand_base`.
    void* Reallocated = Stubs<2>::TermStub<_expand_base>(BaseAddress, Size);
    if (Reallocated && (Flags & 0x8/*HEAP_ZERO_MEMORY*/) == 0) {
      usize Usable = FnReturns0<mi_usable_size>(Reallocated);
      if (Usable < Size) {
        auto* Ptr = ptr_cast<byte>(Reallocated);
        VMemset(Ptr + Size, 0, Size - Usable);
      }
    }
    return Reallocated;
  }
}

//======================================================================//
// Patch Table
//======================================================================//

PerFuncPatchData re::rawPatches[kPatchCount + 1] {
  {
    .FunctionName = "_mi_recalloc_ind",
    .TargetName   = "mi_recalloc",
    .Patches      = {
      { .FDOrIAT = &ReAlloc_recalloc }
    }
  }, {
    .FunctionName = "_mi_malloc_ind",
    .TargetName   = "mi_malloc"
  }, {
    .FunctionName = "_mi_calloc_ind",
    .TargetName   = "mi_calloc"
  }, {
    .FunctionName = "_mi_realloc_ind",
    .TargetName   = "mi_realloc"
  }, {
    .FunctionName = "_mi_free_ind",
    .TargetName   = "mi_free"
  }, {
    .FunctionName = "_mi_expand_ind",
    .TargetName   = "mi_expand"
  }, {
    .FunctionName = "_mi_usable_size_ind",
    .TargetName   = "mi_usable_size",
    .Patches      = {
      { .FDOrIAT = VCAST(&FnReturns0<mi_usable_size>) }
    }
  }, {
    .FunctionName = "_mi_new_nothrow_ind",
    .TargetName   = "mi_new_nothrow"
  }, {
    .FunctionName = "_mi_is_in_heap_region_ind",
    .TargetName   = "mi_is_in_heap_region"
  }, {
    .FunctionName = "_mi_malloc_aligned_ind",
    .TargetName   = "mi_malloc_aligned"
  }, {
    .FunctionName = "_mi_realloc_aligned_ind",
    .TargetName   = "mi_realloc_aligned"
  }, {
    .FunctionName = "_mi_aligned_recalloc_ind",
    .TargetName   = "mi_aligned_recalloc"
  }, {
    .FunctionName = "_mi_malloc_aligned_at_ind",
    .TargetName   = "mi_malloc_aligned_at"
  }, {
    .FunctionName = "_mi_realloc_aligned_at_ind",
    .TargetName   = "mi_realloc_aligned_at"
  }, {
    .FunctionName = "_mi_aligned_offset_recalloc_ind",
    .TargetName   = "mi_aligned_offset_recalloc"
  }, {
    .FunctionName = "malloc",
    .TargetName   = "mi_malloc"
  }, {
    .FunctionName = "calloc",
    .TargetName   = "mi_calloc"
  }, {
    .FunctionName = "realloc",
    .TargetName   = "mi_realloc",
    .TermName     = "_mi_realloc_term",
    .TermAddr     = TermFn<_realloc_base, 2>
  }, {
    .FunctionName = "free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TermFn<_aligned_free, 1, false>
  }, {
    .FunctionName = "_expand",
    .TargetName   = "mi_expand",
    .TermName     = "_mi__expand_term",
    .TermAddr     = static_cast<stub_t<2>>(&Stubs<2>::TermStub<_expand_base>)
  }, {
    .FunctionName = "_recalloc",
    .TargetName   = "mi_recalloc",
    .TermName     = "_mi__recalloc_term",
    .TermAddr     = static_cast<stub_t<2>>(&Stubs<2>::TermStub<_recalloc_base>)
  }, {
    .FunctionName = "_msize",
    .TargetName   = "mi_usable_size",
    .TermName     = "_mi__msize_term",
    .TermAddr     = static_cast<stub_t<1>>(&Stubs<1>::TermStub<_msize_base>)
  }, {
    .FunctionName = "aligned_alloc",
    .TargetName   = "mi_aligned_alloc"
  }, {
    .FunctionName = "_aligned_alloc",
    .TargetName   = "mi_aligned_alloc"
  }, {
    .FunctionName = "_malloc_base",
    .TargetName   = "mi_malloc"
  }, {
    .FunctionName = "_calloc_base",
    .TargetName   = "mi_calloc"
  }, {
    .FunctionName = "_realloc_base",
    .TargetName   = "mi_realloc",
    .TermName     = "_mi_realloc_term",
    .TermAddr     = TermFn<_realloc_base, 2>
  }, {
    .FunctionName = "_free_base",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TermFn<_aligned_free, 1, false>
  }, {
    .FunctionName = "_expand_base",
    .TargetName   = "_mi__expand_base",
    .TermName     = "_mi__expand_base_term",
    .TargetAddr   = Fn<_expand_base, 2>,
    .TermAddr     = TermFn<_expand_base, 2>
  }, {
    .FunctionName = "_recalloc_base",
    .TargetName   = "_mi__recalloc_base",
    .TermName     = "_mi__recalloc_base_term",
    .TargetAddr   = Fn<_recalloc_base, 2>,
    .TermAddr     = TermFn<_recalloc_base, 2>
  }, {
    .FunctionName = "_msize_base",
    .TargetName   = "_mi__msize_base",
    .TermName     = "_mi__msize_base_term",
    .TargetAddr   = Fn<_msize_base, 1>,
    .TermAddr     = TermFn<_msize_base, 1>
  }, {
    .FunctionName = "RtlSizeHeap",
    .TargetName   = "_mi_RtlSizeHeap",
    .TargetAddr   = &_mi_RtlSizeHeap,
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &_mi_RtlSizeHeap_RVA
  }, {
    .FunctionName = "RtlFreeHeap",
    .TargetName   = "_mi_RtlFreeHeap",
    .TargetAddr   = &_mi_RtlFreeHeap,
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &_mi_RtlFreeHeap_RVA
  }, {
    .FunctionName = "RtlReAllocateHeap",
    .TargetName   = "_mi_RtlReAllocateHeap",
    .TargetAddr   = &_mi_RtlReAllocateHeap,
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &_mi_RtlReAllocateHeap_RVA
  }, {
    .FunctionName = "_aligned_malloc",
    .TargetName   = "mi_malloc_aligned"
  }, {
    .FunctionName = "_aligned_realloc",
    .TargetName   = "mi_realloc_aligned",
    .TermName     = "_mi__aligned_realloc_term",
    .TermAddr     = TermFn<_aligned_realloc, 3>
  }, {
    .FunctionName = "_aligned_free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TermFn<_aligned_free, 1, false>
  }, {
    .FunctionName = "_aligned_recalloc",
    .TargetName   = "mi_aligned_recalloc",
    .TermName     = "_mi__aligned_recalloc_term",
    .TermAddr     = TermFn<_aligned_recalloc, 4>
  }, {
    .FunctionName = "_aligned_msize",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term",
    .TargetAddr   = Fn<_aligned_msize, 3>,
    .TermAddr     = TermFn<_aligned_msize, 3>
  }, {
    .FunctionName = "_aligned_offset_malloc",
    .TargetName   = "mi_malloc_aligned_at"
  }, {
    .FunctionName = "_aligned_offset_realloc",
    .TargetName   = "mi_realloc_aligned_at",
    .TermName     = "_mi__aligned_offset_realloc_term",
    .TermAddr     = TermFn<_aligned_offset_realloc, 4>
  }, {
    .FunctionName = "_aligned_offset_recalloc",
    .TargetName   = "mi_aligned_offset_recalloc",
    .TermName     = "_mi__aligned_offset_recalloc_term",
    .TermAddr     = TermFn<_aligned_offset_recalloc, 4>
  }, {
    .FunctionName = "_malloc_dbg",
    .TargetName   = "_mi__malloc_dbg",
    .TargetAddr   = Fn<_malloc_dbg, 4>
  }, {
    .FunctionName = "_realloc_dbg",
    .TargetName   = "_mi__realloc_dbg",
    .TargetAddr   = Fn<_realloc_dbg, 5>
  }, {
    .FunctionName = "_calloc_dbg",
    .TargetName   = "_mi__calloc_dbg",
    .TargetAddr   = Fn<_calloc_dbg, 5>
  }, {
    .FunctionName = "_free_dbg",
    .TargetName   = "_mi__free_dbg",
    .TargetAddr   = Fn<_free_dbg, 2, false>
  }, {
    .FunctionName = "_expand_dbg",
    .TargetName   = "_mi__expand_dbg",
    .TermName     = "_mi__expand_dbg_term",
    .TargetAddr   = Fn<_expand_dbg, 5>,
    .TermAddr     = TermFn<_expand_dbg, 5>
  }, {
    .FunctionName = "_recalloc_dbg",
    .TargetName   = "_mi__recalloc_dbg",
    .TermName     = "_mi__recalloc_dbg_term",
    .TargetAddr   = Fn<_recalloc_dbg, 6>,
    .TermAddr     = TermFn<_recalloc_dbg, 6>
  }, {
    .FunctionName = "_msize_dbg",
    .TargetName   = "_mi__msize_dbg",
    .TermName     = "_mi__msize_dbg_term",
    .TargetAddr   = Fn<_msize_dbg, 2>,
    .TermAddr     = TermFn<_msize_dbg, 2>
  }, {
    .FunctionName = "_aligned_malloc_dbg",
    .TargetName   = "_mi__aligned_malloc_dbg",
    .TargetAddr   = Fn<_aligned_malloc_dbg, 4>
  }, {
    .FunctionName = "_aligned_realloc_dbg",
    .TargetName   = "_mi__aligned_realloc_dbg",
    .TermName     = "_mi__aligned_realloc_dbg_term",
    .TargetAddr   = Fn<_aligned_realloc_dbg, 5>,
    .TermAddr     = TermFn<_aligned_realloc_dbg, 5>
  }, {
    .FunctionName = "_aligned_free_dbg",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TermFn<_aligned_free, 1, false>
  }, {
    .FunctionName = "_aligned_msize_dbg",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term",
    .TargetAddr   = Fn<_aligned_msize, 3>,
    .TermAddr     = TermFn<_aligned_msize, 3>
  }, {
    .FunctionName = "_aligned_recalloc_dbg",
    .TargetName   = "_mi__aligned_recalloc_dbg",
    .TermName     = "_mi__aligned_recalloc_dbg_term",
    .TargetAddr   = Fn<_aligned_recalloc_dbg, 5>,
    .TermAddr     = TermFn<_aligned_recalloc_dbg, 5>
  }, {
    .FunctionName = "_aligned_offset_malloc_dbg",
    .TargetName   = "_mi__aligned_offset_malloc_dbg",
    .TargetAddr   = Fn<_aligned_offset_malloc_dbg, 5>
  }, {
    .FunctionName = "_aligned_offset_realloc_dbg",
    .TargetName   = "_mi__aligned_offset_realloc_dbg",
    .TermName     = "_mi__aligned_offset_realloc_dbg_term",
    .TargetAddr   = Fn<_aligned_offset_realloc_dbg, 6>,
    .TermAddr     = TermFn<_aligned_offset_realloc_dbg, 6>
  }, {
    .FunctionName = "_aligned_offset_recalloc_dbg",
    .TargetName   = "_mi__aligned_offset_recalloc_dbg",
    .TermName     = "_mi__aligned_offset_recalloc_dbg_term",
    .TargetAddr   = Fn<_aligned_offset_recalloc_dbg, 7>,
    .TermAddr     = TermFn<_aligned_offset_recalloc_dbg, 7>
  }
};

MutArrayRef<PerFuncPatchData> re::GetPatches() noexcept {
  return {rawPatches, kPatchCount};
}
