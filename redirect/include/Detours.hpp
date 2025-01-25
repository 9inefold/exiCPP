//===- Detours.hpp --------------------------------------------------===//
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
/// The functions actually implementing the patching.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Patches.hpp>
#include <windef.h>

namespace re {
namespace H {
template <bool V, typename Base>
using RefOrPtrType = std::conditional_t<V, Base&, Base*>;
} // namespace H

template <typename T>
using ref_or_ptr_t = H::RefOrPtrType<
  std::is_reference_v<T>,
  std::remove_reference_t<T>
>;

enum class Jmp {
  NearBYTE,
  NearDWORD,
  FarQWORD,
  Unknown,
};

class DetourHandler {
  byte* Func = nullptr;
public:
  DetourHandler(byte* Func) : Func(Func) {}

  template <typename T>
  ref_or_ptr_t<T> get(i64 Off) const {
    using Base = std::remove_reference_t<T>;
    re_assert(Func != nullptr,
      "Invalid function!");
    Base* const Out
      = reinterpret_cast<Base*>(Func + Off);
    if constexpr (std::is_reference_v<T>)
      return *Out;
    else
      return Out;
  }

  /// Returns the type of detour used, or unknown.
  Jmp getJmpKind() const;
  /// Returns the address of the detour, or null.
  byte* getDetouredAddress() const;
  
  byte* data() const { return Func; }
  explicit operator bool() const { return !!Func; }
};

//////////////////////////////////////////////////////////////////////////
// Setup

HINSTANCE FindMimallocAndSetup(
  MutArrayRef<PerFuncPatchData> Patches,
  ArrayRef<const char*> Names,
  bool ForceRedirect);

void PlaceDllAfterNtdllInLoadOrder(HINSTANCE Dll);

//////////////////////////////////////////////////////////////////////////
// Implementation

PatchResult HandlePatching(
  PatchMode Mode,
  MutArrayRef<PerFuncPatchData> Patches);

} // namespace re
