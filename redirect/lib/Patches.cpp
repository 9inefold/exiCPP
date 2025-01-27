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
#include <Mem.hpp>
#include <Strings.hpp>
#include <WinAPI.hpp>
#include <utility>
#include "Interception.hpp"

#define GET_RVA(name) reinterpret_cast<FUNC_TYPE(name)>(name##_RVA)
#define PATCH(name) {{ .FDOrIAT = STUB_PTR(name) }}

using namespace re;

static_assert(sizeof(void*) == sizeof(void(*)(void)));
static_assert(sizeof(PatchData) == 0x50);
static_assert(sizeof(PerFuncPatchData) == 0x178);

static byte* RtlSizeHeap_RVA = nullptr;
static byte* RtlFreeHeap_RVA = nullptr;
static byte* RtlReAllocateHeap_RVA = nullptr;

//////////////////////////////////////////////////////////////////////////
// Stubs

DEFINE_STUB(void*, malloc, usize Size)
DEFINE_STUB(void*, calloc, usize Num, usize Size)
DEFINE_STUB(void*, realloc, void* Ptr, usize Size)
DEFINE_STUB(void, _aligned_free, void* Ptr)
DEFINE_STUB(void*, _expand_base, void* Ptr, usize Size)
DEFINE_STUB(void*, _recalloc_base, void* Ptr, usize Num)

DEFINE_STUB(usize, usable_size, void* Ptr)
DEFINE_STUB(void*, new_nothrow, usize Size)
DEFINE_STUB(bool, in_heap_region, void* Ptr)

// DEFINE_STUB(void*, realloc_aligned,
//   void* Ptr, usize Size, usize Align)
// DEFINE_STUB(void*, aligned_recalloc,
//   void* Ptr, usize Num, usize Size, usize Align)
// DEFINE_STUB(void*, aligned_offset_recalloc,
//   void* Ptr, usize Num, usize Size, usize Align, usize Off)

DEFINE_STUB(void*, malloc_aligned_at,
  usize Size, usize Align, usize Off)
DEFINE_STUB(void*, realloc_aligned_at,
  void* Ptr, usize Size, usize Align, usize Off)

DEFINE_STUB(void*, _aligned_malloc,
  usize Size, usize Align)
DEFINE_STUB(void*, _aligned_realloc,
  void* Ptr, usize Size, usize Align)
DEFINE_STUB(void*, _aligned_recalloc,
  void* Ptr, usize Num, usize Size, usize Align)
DEFINE_STUB(void*, _aligned_offset_realloc,
  void* Ptr, usize Size, usize Align, usize Off)
DEFINE_STUB(void*, _aligned_offset_recalloc,
  void* Ptr, usize Num, usize Size, usize Align, usize Off)

// DEFINE_STUB(void*, _malloc_dbg,
//   usize Size, int Type, const char* File, int Line)
// DEFINE_STUB(void*, _realloc_dbg,
//   void* Ptr, usize Size, int Type, const char* File, int Line)
// DEFINE_STUB(void*, _calloc_dbg,
//   usize Num, usize Size, int Type, const char* File, int Line)

DEFINE_STUB(void*, _aligned_realloc_dbg,
  void* Ptr, usize Size, usize Align, const char* File, int Line)
DEFINE_STUB(void*, _aligned_recalloc_dbg,
  void* Ptr, usize Num, usize Size, usize Align, const char* File, int Line)
DEFINE_STUB(void*, _aligned_offset_realloc_dbg,
  void* Ptr, usize Size, usize Align, usize Off,
  const char* File, int Line)
DEFINE_STUB(void*, _aligned_offset_recalloc_dbg,
  void* Ptr, usize Num, usize Size, usize Align, usize Off,
  const char* File, int Line)

//////////////////////////////////////////////////////////////////////////
// Detours

/*free == aligned_free*/

// No FUNC
DECLARE_TERM(usize, _msize, void* Ptr) {
  if (Ptr && STUB(in_heap_region)(Ptr)) {
    return STUB(usable_size)(Ptr);
  }
  return 0;
}

DECLARE_FUNC(void*, _expand_base, void* Ptr, usize Size) {
  return STUB(_expand_base)(Ptr, Size);
}
DECLARE_TERM(void*, _expand_base, void* Ptr, usize Size) {
  return STUB(_expand_base)(Ptr, Size);
}

DECLARE_FUNC(void*, _recalloc_base, void* Ptr, usize Num) {
  return STUB(_recalloc_base)(Ptr, Num);
}
DECLARE_TERM(void*, _recalloc_base, void* Ptr, usize Num) {
  return STUB(_recalloc_base)(Ptr, Num);
}

DECLARE_FUNC(usize, _msize_base, void* Ptr) {
  return STUB(usable_size)(Ptr);
}
DECLARE_TERM(usize, _msize_base, void* Ptr) {
  return TERM(_msize)(Ptr);
}

DECLARE_MS_FUNC(ULONG, RtlSizeHeap,
 void* HeapHandle, ULONG Flags, void* BaseAddress) {
  if (!STUB(in_heap_region)(BaseAddress)) {
    auto* const SizeHeap = GET_RVA(RtlSizeHeap);
    if (!SizeHeap)
      return 0u;
    return (*SizeHeap)(
      HeapHandle, Flags, BaseAddress);
  }
  return STUB(usable_size)(BaseAddress);
}

DECLARE_MS_FUNC(bool, RtlFreeHeap,
 void* HeapHandle, ULONG Flags, void* BaseAddress) {
  if (BaseAddress == nullptr)
    return true;
  if (!STUB(in_heap_region)(BaseAddress)) {
    auto* const FreeHeap = GET_RVA(RtlFreeHeap);
    if (!FreeHeap)
      return false;
    return (*FreeHeap)(
      HeapHandle, Flags, BaseAddress);
  }
  STUB(_aligned_free)(BaseAddress);
  return true;
}

DECLARE_MS_FUNC(void*, RtlReAllocateHeap,
 void* HeapHandle, ULONG Flags, void* BaseAddress, usize Size) {
  Size &= 0xffffffff;
  if (!BaseAddress || !STUB(in_heap_region)(BaseAddress)) {
    // The default route, calls `RtlFreeHeap`.
    auto* const ReAllocateHeap = GET_RVA(RtlReAllocateHeap);
    if (!ReAllocateHeap)
      return nullptr;
    return (*ReAllocateHeap)(
      HeapHandle, Flags, BaseAddress, Size);
  } else if ((Flags & 0x10/*HEAP_REALLOC_IN_PLACE_ONLY*/) == 0) {
    // The secondary route, calls `_recalloc_base`.
    if ((Flags & 0x8/*HEAP_ZERO_MEMORY*/) == 0) {
      return STUB(realloc)(BaseAddress, Size);
    }
    return STUB(_recalloc_base)(BaseAddress, Size);
  } else {
    // The final route, calls `_expand_base`.
    void* Reallocated = STUB(_expand_base)(BaseAddress, Size);
    if (Reallocated && (Flags & 0x8/*HEAP_ZERO_MEMORY*/) == 0) {
      usize Usable = STUB(usable_size)(Reallocated);
      if (Usable < Size) {
        auto* Ptr = ptr_cast<byte>(Reallocated);
        VMemset(Ptr + Size, 0, Size - Usable);
      }
    }
    return Reallocated;
  }
}

// No FUNC
DECLARE_TERM(void, _aligned_free, void* Ptr) {
  if (Ptr && STUB(in_heap_region)(Ptr)) {
    return STUB(_aligned_free)(Ptr);
  }
}

DECLARE_FUNC(usize, _aligned_msize, void* Ptr) {
  return STUB(usable_size)(Ptr);
}
DECLARE_TERM(usize, _aligned_msize, void* Ptr) {
  return TERM(_msize)(Ptr);
}

// No TERM
DECLARE_FUNC(void*, _malloc_dbg,
 usize Size, int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(malloc)(Size);
}

// No TERM
DECLARE_FUNC(void*, _realloc_dbg,
 void* Ptr, usize Size,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(realloc)(Ptr, Size);
}

// No TERM
DECLARE_FUNC(void*, _calloc_dbg,
 usize Num, usize Size, int Type,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(calloc)(Num, Size);
}

// No TERM
DECLARE_FUNC(void, _free_dbg, void* Ptr, int /*Type*/) {
  return STUB(_aligned_free)(Ptr);
}

DECLARE_FUNC(void*, _expand_dbg,
 void* Ptr, usize Size,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(_expand_base)(Ptr, Size);
}
DECLARE_TERM(void*, _expand_dbg,
 void* Ptr, usize Size,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(_expand_base)(Ptr, Size);
}

DECLARE_FUNC(void*, _recalloc_dbg,
 void* Ptr, usize Num, usize Size,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(_recalloc_base)(Ptr, Num);
}
DECLARE_TERM(void*, _recalloc_dbg,
 void* Ptr, usize Num, usize Size,
 int /*Type*/, const char* /*File*/, int /*Line*/) {
  return STUB(_recalloc_base)(Ptr, Num);
}

DECLARE_FUNC(usize, _msize_dbg, void* Ptr) {
  return STUB(usable_size)(Ptr);
}
DECLARE_TERM(usize, _msize_dbg, void* Ptr) {
  return TERM(_msize)(Ptr);
}

// No TERM
DECLARE_FUNC(void*, _aligned_malloc_dbg,
 usize Size, usize Align, const char* /*File*/, int /*Line*/) {
  return STUB(_aligned_malloc)(Size, Align);
}

// TERM == STUB
DECLARE_FUNC(void*, _aligned_realloc_dbg,
 void* Ptr, usize Size, usize Align, const char* /*File*/, int /*Line*/) {
  return STUB(_aligned_realloc)(Ptr, Size, Align);
}

/*_aligned_free_dbg == aligned_free*/
/*_aligned_msize_dbg == _aligned_msize*/

// TERM == STUB
DECLARE_FUNC(void*, _aligned_recalloc_dbg,
 void* Ptr, usize Num, usize Size, usize Align,
 const char* /*File*/, int /*Line*/) {
  return STUB(_aligned_recalloc)(Ptr, Num, Size, Align);
}

// No TERM
DECLARE_FUNC(void*, _aligned_offset_malloc_dbg,
 usize Size, usize Align, usize Off,
 const char* /*File*/, int /*Line*/) {
  return STUB(malloc_aligned_at)(Size, Align, Off);
}

// TERM == STUB
DECLARE_FUNC(void*, _aligned_offset_realloc_dbg,
 void* Ptr, usize Size, usize Align, usize Off,
 const char* /*File*/, int /*Line*/) {
  return STUB(realloc_aligned_at)(Ptr, Size, Align, Off);
}

// TERM == STUB
DECLARE_FUNC(void*, _aligned_offset_recalloc_dbg,
 void* Ptr, usize Num, usize Size, usize Align, usize Off,
 const char* /*File*/, int /*Line*/) {
  return STUB(_aligned_offset_recalloc)(Ptr, Num, Size, Align, Off);
}

//======================================================================//
// Patch Table
//======================================================================//

PerFuncPatchData re::rawPatches[kPatchCount + 1] {
  {
    .FunctionName = "_mi_recalloc_ind",
    .TargetName   = "mi_recalloc",
    .Patches      = PATCH(_recalloc_base)
  }, {
    .FunctionName = "_mi_malloc_ind",
    .TargetName   = "mi_malloc",
    .Patches      = PATCH(malloc)
  }, {
    .FunctionName = "_mi_calloc_ind",
    .TargetName   = "mi_calloc",
    .Patches      = PATCH(calloc)
  }, {
    .FunctionName = "_mi_realloc_ind",
    .TargetName   = "mi_realloc",
    .Patches      = PATCH(realloc)
  }, {
    .FunctionName = "_mi_free_ind",
    .TargetName   = "mi_free",
    .Patches      = PATCH(_aligned_free)
  }, {
    .FunctionName = "_mi_expand_ind",
    .TargetName   = "mi_expand",
    .Patches      = PATCH(_expand_base)
  }, {
    .FunctionName = "_mi_usable_size_ind",
    .TargetName   = "mi_usable_size",
    .Patches      = PATCH(usable_size)
  }, {
    .FunctionName = "_mi_new_nothrow_ind",
    .TargetName   = "mi_new_nothrow",
    .Patches      = PATCH(new_nothrow)
  }, {
    .FunctionName = "_mi_is_in_heap_region_ind",
    .TargetName   = "mi_is_in_heap_region",
    .Patches      = PATCH(in_heap_region)
  }, {
    .FunctionName = "_mi_malloc_aligned_ind",
    .TargetName   = "mi_malloc_aligned",
    .Patches      = PATCH(_aligned_malloc)
  }, {
    .FunctionName = "_mi_realloc_aligned_ind",
    .TargetName   = "mi_realloc_aligned",
    .Patches      = PATCH(_aligned_realloc)
  }, {
    .FunctionName = "_mi_aligned_recalloc_ind",
    .TargetName   = "mi_aligned_recalloc",
    .Patches      = PATCH(_aligned_recalloc)
  }, {
    .FunctionName = "_mi_malloc_aligned_at_ind",
    .TargetName   = "mi_malloc_aligned_at",
    .Patches      = PATCH(malloc_aligned_at)
  }, {
    .FunctionName = "_mi_realloc_aligned_at_ind",
    .TargetName   = "mi_realloc_aligned_at",
    .Patches      = PATCH(realloc_aligned_at)
  }, {
    .FunctionName = "_mi_aligned_offset_recalloc_ind",
    .TargetName   = "mi_aligned_offset_recalloc",
    .Patches      = PATCH(_aligned_offset_recalloc)
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
    .TermAddr     = STUB_PTR(realloc)
  }, {
    .FunctionName = "free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TERM_PTR(_aligned_free)
  }, {
    .FunctionName = "_expand",
    .TargetName   = "mi_expand",
    .TermName     = "_mi__expand_term",
    .TermAddr     = STUB_PTR(_expand_base)
  }, {
    .FunctionName = "_recalloc",
    .TargetName   = "mi_recalloc",
    .TermName     = "_mi__recalloc_term",
    .TermAddr     = STUB_PTR(_recalloc_base)
  }, {
    .FunctionName = "_msize",
    .TargetName   = "mi_usable_size",
    .TermName     = "_mi__msize_term",
    .TermAddr     = TERM_PTR(_msize)
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
    .TermAddr     = STUB_PTR(realloc)
  }, {
    .FunctionName = "_free_base",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TERM_PTR(_aligned_free)
  }, {
    .FunctionName = "_expand_base",
    .TargetName   = "_mi__expand_base",
    .TermName     = "_mi__expand_base_term",
    .TargetAddr   = FUNC_PTR(_expand_base),
    .TermAddr     = TERM_PTR(_expand_base)
  }, {
    .FunctionName = "_recalloc_base",
    .TargetName   = "_mi__recalloc_base",
    .TermName     = "_mi__recalloc_base_term",
    .TargetAddr   = FUNC_PTR(_recalloc_base),
    .TermAddr     = TERM_PTR(_recalloc_base)
  }, {
    .FunctionName = "_msize_base",
    .TargetName   = "_mi__msize_base",
    .TermName     = "_mi__msize_base_term",
    .TargetAddr   = FUNC_PTR(_msize_base),
    .TermAddr     = TERM_PTR(_msize_base)
  }, {
    .FunctionName = "RtlSizeHeap",
    .TargetName   = "_mi_RtlSizeHeap",
    .TargetAddr   = FUNC_PTR(RtlSizeHeap),
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &RtlSizeHeap_RVA
  }, {
    .FunctionName = "RtlFreeHeap",
    .TargetName   = "_mi_RtlFreeHeap",
    .TargetAddr   = FUNC_PTR(RtlFreeHeap),
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &RtlFreeHeap_RVA
  }, {
    .FunctionName = "RtlReAllocateHeap",
    .TargetName   = "_mi_RtlReAllocateHeap",
    .TargetAddr   = FUNC_PTR(RtlReAllocateHeap),
    .ModuleName   = "ntdll.dll",
    .FunctionRVA  = &RtlReAllocateHeap_RVA
  }, {
    .FunctionName = "_aligned_malloc",
    .TargetName   = "mi_malloc_aligned"
  }, {
    .FunctionName = "_aligned_realloc",
    .TargetName   = "mi_realloc_aligned",
    .TermName     = "_mi__aligned_realloc_term",
    .TermAddr     = STUB_PTR(_aligned_realloc)
  }, {
    .FunctionName = "_aligned_free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TERM_PTR(_aligned_free)
  }, {
    .FunctionName = "_aligned_recalloc",
    .TargetName   = "mi_aligned_recalloc",
    .TermName     = "_mi__aligned_recalloc_term",
    .TermAddr     = STUB_PTR(_aligned_recalloc)
  }, {
    .FunctionName = "_aligned_msize",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term",
    .TargetAddr   = FUNC_PTR(_aligned_msize),
    .TermAddr     = TERM_PTR(_aligned_msize)
  }, {
    .FunctionName = "_aligned_offset_malloc",
    .TargetName   = "mi_malloc_aligned_at"
  }, {
    .FunctionName = "_aligned_offset_realloc",
    .TargetName   = "mi_realloc_aligned_at",
    .TermName     = "_mi__aligned_offset_realloc_term",
    .TermAddr     = STUB_PTR(_aligned_offset_realloc)
  }, {
    .FunctionName = "_aligned_offset_recalloc",
    .TargetName   = "mi_aligned_offset_recalloc",
    .TermName     = "_mi__aligned_offset_recalloc_term",
    .TermAddr     = STUB_PTR(_aligned_offset_recalloc)
  }, {
    .FunctionName = "_malloc_dbg",
    .TargetName   = "_mi__malloc_dbg",
    .TargetAddr   = FUNC_PTR(_malloc_dbg)
  }, {
    .FunctionName = "_realloc_dbg",
    .TargetName   = "_mi__realloc_dbg",
    .TargetAddr   = FUNC_PTR(_realloc_dbg)
  }, {
    .FunctionName = "_calloc_dbg",
    .TargetName   = "_mi__calloc_dbg",
    .TargetAddr   = FUNC_PTR(_calloc_dbg)
  }, {
    .FunctionName = "_free_dbg",
    .TargetName   = "_mi__free_dbg",
    .TargetAddr   = FUNC_PTR(_free_dbg)
  }, {
    .FunctionName = "_expand_dbg",
    .TargetName   = "_mi__expand_dbg",
    .TermName     = "_mi__expand_dbg_term",
    .TargetAddr   = FUNC_PTR(_expand_dbg),
    .TermAddr     = TERM_PTR(_expand_dbg)
  }, {
    .FunctionName = "_recalloc_dbg",
    .TargetName   = "_mi__recalloc_dbg",
    .TermName     = "_mi__recalloc_dbg_term",
    .TargetAddr   = FUNC_PTR(_recalloc_dbg),
    .TermAddr     = TERM_PTR(_recalloc_dbg)
  }, {
    .FunctionName = "_msize_dbg",
    .TargetName   = "_mi__msize_dbg",
    .TermName     = "_mi__msize_dbg_term",
    .TargetAddr   = FUNC_PTR(_msize_dbg),
    .TermAddr     = TERM_PTR(_msize_dbg)
  }, {
    .FunctionName = "_aligned_malloc_dbg",
    .TargetName   = "_mi__aligned_malloc_dbg",
    .TargetAddr   = FUNC_PTR(_aligned_malloc_dbg)
  }, {
    .FunctionName = "_aligned_realloc_dbg",
    .TargetName   = "_mi__aligned_realloc_dbg",
    .TermName     = "_mi__aligned_realloc_dbg_term",
    .TargetAddr   = FUNC_PTR(_aligned_realloc_dbg),
    .TermAddr     = STUB_PTR(_aligned_realloc_dbg)
  }, {
    .FunctionName = "_aligned_free_dbg",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term",
    .TermAddr     = TERM_PTR(_aligned_free)
  }, {
    .FunctionName = "_aligned_msize_dbg",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term",
    .TargetAddr   = FUNC_PTR(_aligned_msize),
    .TermAddr     = TERM_PTR(_aligned_msize)
  }, {
    .FunctionName = "_aligned_recalloc_dbg",
    .TargetName   = "_mi__aligned_recalloc_dbg",
    .TermName     = "_mi__aligned_recalloc_dbg_term",
    .TargetAddr   = FUNC_PTR(_aligned_recalloc_dbg),
    .TermAddr     = STUB_PTR(_aligned_recalloc_dbg)
  }, {
    .FunctionName = "_aligned_offset_malloc_dbg",
    .TargetName   = "_mi__aligned_offset_malloc_dbg",
    .TargetAddr   = FUNC_PTR(_aligned_offset_malloc_dbg)
  }, {
    .FunctionName = "_aligned_offset_realloc_dbg",
    .TargetName   = "_mi__aligned_offset_realloc_dbg",
    .TermName     = "_mi__aligned_offset_realloc_dbg_term",
    .TargetAddr   = FUNC_PTR(_aligned_offset_realloc_dbg),
    .TermAddr     = STUB_PTR(_aligned_offset_realloc_dbg)
  }, {
    .FunctionName = "_aligned_offset_recalloc_dbg",
    .TargetName   = "_mi__aligned_offset_recalloc_dbg",
    .TermName     = "_mi__aligned_offset_recalloc_dbg_term",
    .TargetAddr   = FUNC_PTR(_aligned_offset_recalloc_dbg),
    .TermAddr     = STUB_PTR(_aligned_offset_recalloc_dbg)
  }
};

MutArrayRef<PerFuncPatchData> re::GetPatches() noexcept {
  return {rawPatches, kPatchCount};
}
