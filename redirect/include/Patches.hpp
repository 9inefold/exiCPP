//===- Patches.hpp --------------------------------------------------===//
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
/// This file declares the interface for patching/detouring functions.
///
//===----------------------------------------------------------------===//

#pragma once

#include <ArrayRef.hpp>
#include <Fundamental.hpp>

namespace re {

enum PatchMode : int {
  PM_UNPATCH    = 0,
  PM_PATCH      = 1,
  PM_PATCH_TERM = 2,
};

enum PatchResult : int {
  PR_SUCCESS = 0,
  PR_FAILED  = 1,
  PR_PARTIAL = 2,
};

struct PatchData {
  int UsePatchedImports;
  // int _iPad1;
  union {
    byte* FunctionData;
    void** IATEntry;
    void* FDOrIAT;
  };
  PatchMode ModeStore;
  // int _iPad2;  
  byte* StoreFunc;        // Stored in .text section
  usize StoreSize; 
  i64 FunctionOffset;     // Either FunctionData or StoreFunc
  byte* FunctionAddr;
  usize JmpSize; 
  byte PatchBytes[16]; 
};

inline constexpr usize kPatchDataCount = 4;
struct PerFuncPatchData {
  const char* FunctionName;
  const char* TargetName;
  const char* TermName;
  void*       TargetAddr;
  void*       TermAddr;
  const char* ModuleName;
  byte**      FunctionRVA;
  PatchData   Patches[kPatchDataCount];
};

inline constexpr usize kPatchCount = 56;
extern PerFuncPatchData rawPatches[];
MutArrayRef<PerFuncPatchData> GetPatches() noexcept;

} // namespace re
