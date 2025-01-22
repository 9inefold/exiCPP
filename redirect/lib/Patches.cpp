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

using namespace re;

static_assert(sizeof(PatchData) == 0x50);
static_assert(sizeof(PerFuncPatchData) == 0x178);

PerFuncPatchData re::rawPatches[kPatchCount + 1] {
  {
    .FunctionName = "_mi_recalloc_ind",
    .TargetName   = "mi_recalloc"
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
    .TargetName   = "mi_usable_size"
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
    .TermName     = "_mi_realloc_term"
  }, {
    .FunctionName = "free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term"
  }, {
    .FunctionName = "_expand",
    .TargetName   = "mi_expand",
    .TermName     = "_mi__expand_term"
  }, {
    .FunctionName = "_recalloc",
    .TargetName   = "mi_recalloc",
    .TermName     = "_mi__recalloc_term"
  }, {
    .FunctionName = "_msize",
    .TargetName   = "mi_usable_size",
    .TermName     = "_mi__msize_term"
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
    .TermName     = "_mi_realloc_term"
  }, {
    .FunctionName = "_free_base",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term"
  }, {
    .FunctionName = "_expand_base",
    .TargetName   = "_mi__expand_base",
    .TermName     = "_mi__expand_base_term"
  }, {
    .FunctionName = "_recalloc_base",
    .TargetName   = "_mi__recalloc_base",
    .TermName     = "_mi__recalloc_base_term"
  }, {
    .FunctionName = "_msize_base",
    .TargetName   = "_mi__msize_base",
    .TermName     = "_mi__msize_base_term"
  }, {
    .FunctionName = "RtlSizeHeap",
    .TargetName   = "_mi_RtlSizeHeap",
    .ModuleName   = "ntdll.dll"
  }, {
    .FunctionName = "RtlFreeHeap",
    .TargetName   = "_mi_RtlFreeHeap",
    .ModuleName   = "ntdll.dll"
  }, {
    .FunctionName = "RtlReAllocateHeap",
    .TargetName   = "_mi_RtlReAllocateHeap",
    .ModuleName   = "ntdll.dll"
  }, {
    .FunctionName = "_aligned_malloc",
    .TargetName   = "mi_malloc_aligned"
  }, {
    .FunctionName = "_aligned_realloc",
    .TargetName   = "mi_realloc_aligned",
    .TermName     = "_mi__aligned_realloc_term"
  }, {
    .FunctionName = "_aligned_free",
    .TargetName   = "mi_free",
    .TermName     = "_mi_free_term"
  }, {
    .FunctionName = "_aligned_recalloc",
    .TargetName   = "mi_aligned_recalloc",
    .TermName     = "_mi__aligned_recalloc_term"
  }, {
    .FunctionName = "_aligned_msize",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term"
  }, {
    .FunctionName = "_aligned_offset_malloc",
    .TargetName   = "mi_malloc_aligned_at"
  }, {
    .FunctionName = "_aligned_offset_realloc",
    .TargetName   = "mi_realloc_aligned_at",
    .TermName     = "_mi__aligned_offset_realloc_term"
  }, {
    .FunctionName = "_aligned_offset_recalloc",
    .TargetName   = "mi_aligned_offset_recalloc",
    .TermName     = "_mi__aligned_offset_recalloc_term"
  }, {
    .FunctionName = "_malloc_dbg",
    .TargetName   = "_mi__malloc_dbg"
  }, {
    .FunctionName = "_realloc_dbg",
    .TargetName   = "_mi__realloc_dbg"
  }, {
    .FunctionName = "_calloc_dbg",
    .TargetName   = "_mi__calloc_dbg"
  }, {
    .FunctionName = "_free_dbg",
    .TargetName   = "_mi__free_dbg"
  }, {
    .FunctionName = "_expand_dbg",
    .TargetName   = "_mi__expand_dbg",
    .TermName     = "_mi__expand_dbg_term"
  }, {
    .FunctionName = "_recalloc_dbg",
    .TargetName   = "_mi__recalloc_dbg",
    .TermName     = "_mi__recalloc_dbg_term"
  }, {
    .FunctionName = "_msize_dbg",
    .TargetName   = "_mi__msize_dbg",
    .TermName     = "_mi__msize_dbg_term"
  }, {
    .FunctionName = "_aligned_malloc_dbg",
    .TargetName   = "_mi__aligned_malloc_dbg"
  }, {
    .FunctionName = "_aligned_realloc_dbg",
    .TargetName   = "_mi__aligned_realloc_dbg",
    .TermName     = "_mi__aligned_realloc_dbg_term"
  }, {
    .FunctionName = "_aligned_realloc_dbg",
    .TargetName   = "_mi__aligned_realloc_dbg",
    .TermName     = "_mi__aligned_realloc_dbg_term"
  }, {
    .FunctionName = "_aligned_msize_dbg",
    .TargetName   = "_mi__aligned_msize",
    .TermName     = "_mi__aligned_msize_term"
  }, {
    .FunctionName = "_aligned_recalloc_dbg",
    .TargetName   = "_mi__aligned_recalloc_dbg",
    .TermName     = "_mi__aligned_recalloc_dbg_term"
  }, {
    .FunctionName = "_aligned_offset_malloc_dbg",
    .TargetName   = "_mi__aligned_offset_malloc_dbg"
  }, {
    .FunctionName = "_aligned_offset_realloc_dbg",
    .TargetName   = "_mi__aligned_offset_realloc_dbg",
    .TermName     = "_mi__aligned_offset_realloc_dbg_term"
  }, {
    .FunctionName = "_aligned_offset_recalloc_dbg",
    .TargetName   = "_mi__aligned_offset_recalloc_dbg",
    .TermName     = "_mi__aligned_offset_recalloc_dbg_term"
  }
};

MutArrayRef<PerFuncPatchData> re::GetPatches() noexcept {
  return {rawPatches, kPatchCount};
}
