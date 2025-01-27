//===- DetoursImpl.cpp ----------------------------------------------===//
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
/// This file implements the detours.
///
//===----------------------------------------------------------------===//

#include <Detours.hpp>
#include <Buf.hpp>
#include <Globals.hpp>
#include <Logging.hpp>
#include <RVA.hpp>
#include <Strings.hpp>
#include <Version.hpp>
#include <limits>
#include "NtImports.hpp"

using namespace re;

namespace { struct Po; }
static PatchMode LastMode = PM_UNPATCH;

static HANDLE GetProgramHandle() {
  constexpr usize Val = 0xffffffffffffffff;
  return reinterpret_cast<HANDLE>(Val);
}

static bool ChangeProtect(
 void* Base, usize InSize,
 DWORD Flags, DWORD* OldFlags = nullptr) {
  if (!OldFlags)
    OldFlags = &Flags;
  i32 Status = NtProtectVirtualMemory(
    GetProgramHandle(), &Base, &InSize, Flags, OldFlags);
  return Status >= 0;
}

static bool InstallDetourInAddressStore(byte* AddressStore, void* Detour) {
  DWORD OldFlags = 4;
  if (!ChangeProtect(AddressStore, 8, PAGE_EXECUTE_READWRITE, &OldFlags)) {
    MiError("unable to get write permission for address store (at %p)",
      AddressStore);
    return false;
  }
  *ptr_cast<void*>(AddressStore) = Detour;
  ChangeProtect(AddressStore, 8, OldFlags);
  return true;
}

namespace {

class PatchHandler {
  class Restorer;
  PatchMode Mode;
public:
  PatchHandler(PatchMode Mode) : Mode(Mode) {}
  bool operator()(PerFuncPatchData& Patch);
private:
  bool handlePatch(PerFuncPatchData& Patch, i32 At) const;
  static void unpatch(PatchData& Patch);
  static void patch(PatchData& Patch, void* Addr); 
  static void patchFunction(PatchData& Patch, byte* Detour); 
  static void patchNear(PatchData& Patch, i64 Dist); 
  static void patchFar(PatchData& Patch, Po* Detour); 
};

class PatchHandler::Restorer {
  PatchHandler& self;
  PatchMode OldMode;
public:
  Restorer(PatchHandler& self) :
   self(self), OldMode(self.Mode) {
  }
  ~Restorer() {
    self.Mode = OldMode;
  }
};

//////////////////////////////////////////////////////////////////////////
// Implementation

static void SaveBytesForPatching(PatchData& Patch, void* Addr, usize JmpSize) {
  if (Patch.FDOrIAT == nullptr)
    return;
  if (JmpSize == 0 || Patch.JmpSize != 0)
    return;

  if (JmpSize > 16) {
    MiError("trying to save beyond the maximum jump size: %zu > %zu",
      JmpSize, 16);
    JmpSize = 16;
  }
  Patch.JmpSize = JmpSize;
  if (Addr != nullptr)
    Patch.FDOrIAT = Addr;
  
  VMemcpy(Patch.PatchBytes, Patch.FDOrIAT, Patch.JmpSize);
}
static inline void SaveBytesForPatching(PatchData& Patch, usize JmpSize) {
  SaveBytesForPatching(Patch, nullptr, JmpSize);
}

static bool IsNearCall(i64 Dist) {
  // This is used for inline relative jumps.
  return (Dist < 0x7ffffff0) && (Dist > -0x7ffffff1);
}

/// Loops backwards from the entry to a function to check for padding bytes.
/// INT3 (or `0xCC`) is commonly used to pad out data between functions, as it
/// causes `SIGILL` to be raised. The nop may also be generated in release.
static i64 CountNoopAndInt3Padding(byte* Data, i64 MaxIters) {
  for (int Ix = 0; Ix < MaxIters; ++Ix) {
    const byte Instr = Data[-1 - Ix];
    // Check if byte isn't INT3 or NOP.
    if (Instr != 0xCC && Instr != 0x90)
      // Unexpected byte, return.
      return Ix;
  }

  return MaxIters;
}

static bool IsAllInt3UpTo(byte* Store, i64 MaxIters) {
  for (int Ix = 0; Ix < MaxIters; ++Ix) {
    if (Store[Ix] != 0xCC)
      return false;
  }
  return true;
}

static byte* GetAndSetAddressStore(byte* Store, i64 Size, void* Detour) {
  constexpr i64 kStoreSize = 18;
  if (!Store || Size < kStoreSize)
    return nullptr;
  for (i64 Ix = 0; Ix < Size - kStoreSize; ++Ix) {
    byte* Ptr = Store + Ix;
    if (IsAllInt3UpTo(Ptr, kStoreSize)) {
      byte* AlignedStore = align_ptr<8>(Ptr);
      if (InstallDetourInAddressStore(AlignedStore, Detour))
        return AlignedStore;
    }
  }

  return nullptr;
}

void PatchHandler::patchNear(PatchData& Patch, const i64 Dist) {
  i64 Padding = CountNoopAndInt3Padding(Patch.FunctionData, 5);
  DetourHandler DH(Patch.FunctionData);
  if (Padding < 5) {
    SaveBytesForPatching(Patch, DH.data(), 5);
    // ------------
    // jmp DWORD imm @Dist
    DH.get<u8&>(0)   = 0xe9;
    DH.get<i32&>(1)  = Dist - 5; // Relative to entry
    MiTraceEx("installed near < 5; %li", Padding);
  } else /*Padding == 5*/ {
    SaveBytesForPatching(Patch, DH.data() - 5, 7);
    // jmp DWORD imm @Dist
    DH.get<u8&>(-5)  = 0xe9;
    DH.get<i32&>(-4) = Dist;
    // ------------
    // jmp BYTE imm -7
    DH.get<u8&>(0)   = 0xeb;
    DH.get<i8&>(1)   = -7;
    MiTraceEx("installed near == 5; %li", Padding);
  }

  MiTrace("write entry: %p, %i, 0x%zx, na",
    DH.data(), Padding, DH.data() + Dist);
}

void PatchHandler::patchFar(PatchData& Patch, Po* Detour) {
  i64 Padding = CountNoopAndInt3Padding(Patch.FunctionData, 14);
  DetourHandler DH(Patch.FunctionData);
  if (Padding < 8) {
    if (Patch.FunctionOffset == 0 && Patch.StoreFunc) {
      byte* Store = GetAndSetAddressStore(
        Patch.StoreFunc, Patch.StoreSize, Detour);
      if (Store != nullptr) {
        Patch.FunctionOffset = (Store - DH.data());
      }
    }

    if (Patch.FunctionOffset == 0) {
      SaveBytesForPatching(Patch, DH.data(), 14);
      // ------------
      // jmp QWORD PTR [rip + 0]
      // [@func]
      DH.get<u8&>(0)  = 0xff;
      DH.get<u8&>(1)  = 0x25;
      DH.get<i32&>(2) = 0;
      DH.get<Po*&>(6) = Detour;
      MiTraceEx("installed far < 8, off == 0; %li", Padding);
    } else {
      SaveBytesForPatching(Patch, DH.data(), 6);
      // ------------
      // jmp QWORD PTR [rip - 6 + @store]
      DH.get<u8&>(0)  = 0xff;
      DH.get<i8&>(1)  = 0x25;
      DH.get<i32&>(2) = Patch.FunctionOffset - 6; // @store relative
      MiTraceEx("installed far < 8, off != 0; %li", Padding);
    }
  } else if (Padding < 14) {
    SaveBytesForPatching(Patch, DH.data() - 8, 14);
    // [@func]
    DH.get<Po*&>(-8)  = Detour;
    // ------------
    // jmp QWORD PTR [rip - 14]
    DH.get<u8&>(0)    = 0xff;
    DH.get<u8&>(1)    = 0x25;
    DH.get<i32&>(2)   = -14;  // Relative to next instruction
    MiTraceEx("installed far < 14; %li", Padding);
  } else /*Padding == 14*/ {
    SaveBytesForPatching(Patch, DH.data() - 14, 16);
    // jmp QWORD PTR [rip + 0]
    // [@func]
    DH.get<u8&>(-14)  = 0xff;
    DH.get<u8&>(-13)  = 0x25;
    DH.get<i32&>(-12) = 0;
    DH.get<Po*&>(-8)  = Detour;
    // ------------
    // jmp BYTE imm -16
    DH.get<u8&>(0)    = 0xeb;
    DH.get<i8&>(1)    = -16;  // Relative to next instruction
    MiTraceEx("installed far == 14; %li", Padding);
  }

  MiTrace("write entry: %p, %i, 0x%zx, %zi",
    DH.data(), Padding, Detour, Patch.FunctionOffset);
}

void PatchHandler::patchFunction(PatchData& Patch, byte* Detour) {
  re_assert(Patch.FunctionData, "should always be true");
  const i64 Dist = (Detour - Patch.FunctionData);
  if (IsNearCall(Dist)) {
    PatchHandler::patchNear(Patch, Dist);
  } else {
    // Using `Po` to keep names shorter :P
    PatchHandler::patchFar(Patch, ptr_cast<Po>(Detour));
  }
}

void PatchHandler::patch(PatchData& Patch, void* Addr) {
  if (Addr == nullptr)
    return;
  if (!Patch.FDOrIAT)
    return;
  
  if (not Patch.UsePatchedImports) {
    patchFunction(Patch, static_cast<byte*>(Addr));
  } else {
    SaveBytesForPatching(
      Patch, sizeof(*Patch.IATEntry));
    *Patch.IATEntry = Addr;
    MiTraceEx("installed import 0x%zx", *Patch.IATEntry);
  }
}

void PatchHandler::unpatch(PatchData& Patch) {
  if (Patch.JmpSize == 0 || !Patch.FDOrIAT)
    return;
  VMemcpy(Patch.FDOrIAT,
    Patch.PatchBytes, Patch.JmpSize);
}

//////////////////////////////////////////////////////////////////////////
// Setup

bool PatchHandler::handlePatch(PerFuncPatchData& Data, i32 At) const {
  if UNLIKELY(At < 0 || At >= kPatchDataCount)
    return false;

  PatchData& Patch = Data.Patches[At];
  if (Patch.FDOrIAT == nullptr)
    return true;
  // Check if patch mode has already been run.
  if (Patch.ModeStore == Mode)
    return true;
  // Check if the jmp is trivial.
  if (Mode == PM_UNPATCH && Patch.JmpSize == 0) {
    Patch.ModeStore = PM_UNPATCH;
    return true;
  }

  DWORD OldFlags = 4;
  byte* BaseAddr = Patch.FunctionData - 16;
  constexpr usize Size = 32;
  if (!ChangeProtect(BaseAddr, Size, PAGE_EXECUTE_READWRITE, &OldFlags)) {
    MiError("unable to patch %s (%p); unable to get write permission",
      Data.FunctionName, Patch.FDOrIAT);
    return false;
  }

  if (Mode == PM_UNPATCH)
    PatchHandler::unpatch(Patch);
  else if (Mode == PM_PATCH)
    PatchHandler::patch(Patch, Data.TargetAddr);
  else /*Mode == PM_PATCH_TERM*/
    PatchHandler::patch(Patch, Data.TermAddr);
  
  Patch.ModeStore = Mode;
  ChangeProtect(BaseAddr, Size, OldFlags);
  return true;
}

bool PatchHandler::operator()(PerFuncPatchData& Data) {
  if (Data.Patches[0].FDOrIAT == nullptr)
    return true;
  
  Restorer R(*this);
  if (Mode == PM_PATCH_TERM && !Data.TermName) {
    // This will be restored when leaving the scope.
    this->Mode = PM_PATCH;
  }

  if (Mode == PM_UNPATCH) {
    bool PatchStatus = true;
    // This works correctly in the original, unlike `ModifyAllPatches`.
    for (i32 Ix = kPatchDataCount - 1; Ix >= 0; --Ix) {
      if (!handlePatch(Data, Ix))
        PatchStatus = false;
    }
    return PatchStatus;
  }

  // Not unpatching, loop forward.
  for (i32 Ix = 0; Ix < kPatchDataCount; ++Ix) {
    if (!handlePatch(Data, Ix))
      return false;
  }

  return true;
}

} // namespace `anonymous`

static bool ModifyAllPatches(
  PatchMode Mode,
  MutArrayRef<PerFuncPatchData> Patches
) {
  if (Mode == ::LastMode)
    return true;
  ::LastMode = Mode;

  PatchHandler Handler(Mode);
  if (Mode == PM_UNPATCH) {
    bool PatchStatus = true;
    // There's actually a bug here in the original. It counts up to
    // `Patches.size() - 1`, but in the loop stops iterating if `Ix == 0`,
    // which means the first patch is never undone. This has been fixed here.
    for (i32 Ix = kPatchCount - 1; Ix >= 0; --Ix) {
      PerFuncPatchData& Patch = Patches[Ix];
      if UNLIKELY(Patch.FunctionName == nullptr)
        break;
      if (!Handler(Patch))
        PatchStatus = false;
    }
    return PatchStatus;
  }

  // Not unpatching, loop forward.
  for (PerFuncPatchData& Patch : Patches) {
    if UNLIKELY(Patch.FunctionName == nullptr)
      break;
    if (!Handler(Patch))
      return false;
  }

  return true;
}

PatchResult re::HandlePatching(
  PatchMode Mode,
  MutArrayRef<PerFuncPatchData> Patches
) {
  if (Patches.size() < kPatchCount)
    return PR_FAILED;

  Patches = Patches.take_front(kPatchCount);
  bool DidSucceed = ModifyAllPatches(Mode, Patches);
  if (!DidSucceed && Mode != PM_UNPATCH) {
    if (!ModifyAllPatches(PM_UNPATCH, Patches)) {
      MiError("unable to roll back partially applied patches!");
      return PR_PARTIAL;
    }
  }

  return DidSucceed ? PR_SUCCESS : PR_FAILED;
}

//======================================================================//
// Exports
//======================================================================//

extern "C" {

DLL_EXPORT bool mi_redirect_enable(void) {
  PatchResult Result
    = HandlePatching(PM_PATCH, GetPatches());
  return (Result == PR_SUCCESS);
}

DLL_EXPORT bool mi_redirect_enable_term(void) {
  PatchResult Result
    = HandlePatching(PM_PATCH_TERM, GetPatches());
  return (Result == PR_SUCCESS);
}

DLL_EXPORT void mi_redirect_disable(void) {
  (void) HandlePatching(PM_UNPATCH, GetPatches());
}

DLL_EXPORT void mi_allocator_done(void) {
  return;
}


} // extern "C"
