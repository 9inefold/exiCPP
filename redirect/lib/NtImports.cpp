//===- Support/Globals.cpp ------------------------------------------===//
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

#include "NtImports.hpp"
#include <RVA.hpp>

using namespace re;

static_assert(sizeof(TLDRListEntry<0>) == sizeof(LDRListEntry));
static_assert(sizeof(AnsiString) == 0x10);
static_assert(sizeof(UnicodeString) == 0x10);
static_assert(sizeof(RTLBalancedNode) == 0x18);
static_assert(sizeof(LDRDataTableEntry) == 0x118);

template struct re::H::GenericString<char>;
template struct re::H::GenericString<wchar_t>;

template struct re::TLDRListEntry<0u>;
template struct re::TLDRListEntry<1u>;
template struct re::TLDRListEntry<2u>;

//////////////////////////////////////////////////////////////////////////
// Base

namespace {
struct Def_PEB_LDR_DATA {
  ULONG             Length;
  BOOLEAN           Initialized;
  PVOID             SsHandle;
  LIST_ENTRY        InLoadOrderModuleList;
  LIST_ENTRY        InMemoryOrderModuleList;
  LIST_ENTRY        InInitializationOrderModuleList;
  PVOID             EntryInProgress;
  BOOLEAN           ShutdownInProgress;
  HANDLE            ShutdownThreadId;
};

struct Def_PEB {
  u8                Reserved[4];
  HANDLE            Mutant;
  HANDLE            ImageBase;
  Def_PEB_LDR_DATA* LDRData;
  // ...
};

struct Def_TIB {
	PVOID             ExceptionList;
	PVOID             StackBase;
	PVOID             StackLimit;
	PVOID             SubSystemTib;
	union {
    PVOID           FiberData;
    DWORD           Version;
	};
	PVOID             ArbitraryUserPointer;
	Def_TIB*          Self;
};

struct Def_TEB {
  Def_TIB           Tib;
  PVOID             EnvironmentPointer;
  struct {
    HANDLE          UniqueProcess;
    HANDLE          UniqueThread;
  };
  PVOID             ActiveRpcHandle;
  PVOID             ThreadLocalStoragePointer;
  Def_PEB*          Peb;
};
} // namespace `anonymous`

/// Load from segment register at `offset`.
ALWAYS_INLINE static void* LoadSegOffset(uptr offset) {
  void* raw_addr = nullptr;
  __asm__(
   "mov %%gs:%[off], %[ret]"
   : [ret] "=r"(raw_addr)
   : [off]  "m"(*reinterpret_cast<u64*>(offset))
  );
  return raw_addr;
}

LDRListEntry* LDRDataTableEntry::GetBase() {
#if HAS_BUILTIN(__builtin_offsetof)
  static constexpr uptr Off = __builtin_offsetof(Def_TIB, Self);
#else
  static const uptr Off = offsetof(NT_TIB, Self);
#endif
  void* const Raw = LoadSegOffset(Off);
  auto* Ldr = ptr_cast<Def_TEB>(Raw)->Peb->LDRData;
  return ptr_cast<LDRListEntry>(&Ldr->InLoadOrderModuleList);
}

LoadOrderList* LDRDataTableEntry::GetLoadOrder() {
  return static_cast<LoadOrderList*>(GetBase() + LDRListKind::loadOrder);
}

MemOrderList* LDRDataTableEntry::GetMemOrder() {
  return static_cast<MemOrderList*>(GetBase() + LDRListKind::memOrder);
}

InitOrderList* LDRDataTableEntry::GetInitOrder() {
  return static_cast<InitOrderList*>(GetBase() + LDRListKind::initOrder);
}

//////////////////////////////////////////////////////////////////////////
// Ldr

InitOrderList* LDRDataTableEntry::inInitOrder() const {
  return this->asListEntry<LDRListKind::initOrder>();
}

MemOrderList*  LDRDataTableEntry::inMemOrder() const {
  return this->asListEntry<LDRListKind::memOrder>();
}

LoadOrderList* LDRDataTableEntry::inLoadOrder() const {
  return this->asListEntry<LDRListKind::loadOrder>();
}

IMAGE_NT_HEADERS64* LDRDataTableEntry::getNtHeader() const {
  RVAHandler Mod(DllBase, SizeOfImage);
  auto* Header = Mod.get<IMAGE_DOS_HEADER>(0);
  return Mod.get<IMAGE_NT_HEADERS64>(Header->e_lfanew);
}
