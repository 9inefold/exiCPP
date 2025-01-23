//===- NtImports.hpp ------------------------------------------------===//
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
/// This file declares the ntdll imports.
/// Adapted from https://github.com/8ightfold/headless-compiler/blob/main/hc-rt/include/Bootstrap/Win64KernelDefs.hpp
///
//===----------------------------------------------------------------===//

#pragma once

#include <Fundamental.hpp>
#include <WinAPI.hpp>

namespace re {

struct LDRListEntry;
struct LDRDataTableEntry;

using NTSTATUS = LONG;
using LDRENUMPROC = void(*)(
  LDRDataTableEntry*  Record,
  void*               Context,
  bool*               ShouldStop);

enum LDRDllLoadReason {
  LR_Unknown                     = -1,
  LR_StaticDependency            = 0, 
  LR_StaticForwarderDependency   = 1, 
  LR_DynamicForwarderDependency  = 2, 
  LR_DelayloadDependency         = 3, 
  LR_DynamicLoad                 = 4,
  LR_AsImageLoad                 = 5, 
  LR_AsDataLoad                  = 6, 
};

struct LDRListKind {
  static constexpr usize loadOrder  = 0U;
  static constexpr usize memOrder   = 1U;
  static constexpr usize initOrder  = 2U;
};

//////////////////////////////////////////////////////////////////////////
// Basic Structures

namespace H {

template <typename Ch>
struct GenericString {
  using Char = Ch;
public:
  u16 Length;        // Length in bytes.
  u16 MaximumLength; // Max length in bytes.
  Char* Buffer;      // Pointer to the buffer.
public:
  constexpr Char* data() { return Buffer; }
  constexpr const Char* data() const { return Buffer; }
  usize size() const { return Length / sizeof(Ch); }
  usize sizeInBytes() const { return Length; }
  usize capacity() const { return MaximumLength / sizeof(Ch); }
  usize capacityInBytes() const { return MaximumLength; }
  bool empty() const { return !Length || !Buffer; }
};

extern template struct GenericString<char>;
extern template struct GenericString<wchar_t>;

} // namespace H

struct AnsiString : public H::GenericString<char> {};
struct UnicodeString : public H::GenericString<wchar_t> {};

struct RTLBalancedNode {
  union {
    RTLBalancedNode* Children[2];
    struct {
      RTLBalancedNode* Left;
      RTLBalancedNode* Right;
    };
  };
  union {
    struct {
      u8 Red : 1;
      u8 Balance : 2;
    };
    u32 ParentValue;
  };
};

//////////////////////////////////////////////////////////////////////////
// Type-Safe List

struct LDRListEntry {
  LDRListEntry* _iPrev;
  LDRListEntry* _iNext;
};

template <usize TableOffset> struct TLDRListEntry;
using LoadOrderList = TLDRListEntry<LDRListKind::loadOrder>;
using MemOrderList  = TLDRListEntry<LDRListKind::memOrder>;
using InitOrderList = TLDRListEntry<LDRListKind::initOrder>;

struct LDRDataTableEntry {
  LDRListEntry      _iInLoadOrderLinks; 
  LDRListEntry      _iInMemoryOrderLinks; 
  LDRListEntry      _iInInitializationOrderLinks; 
  byte*             DllBase; 
  void*             EntryPoint; 
  u32               SizeOfImage; 
  UnicodeString     FullDllName; 
  UnicodeString     BaseDllName; 
  u32               Flags; 
  i16               LoadCount; 
  i16               TlsIndex; 
  LIST_ENTRY        HashLinks; 
  u32               TimeDateStamp; 
  HANDLE            ActivationContext; 
  void*             Lock;
  void*             DdagNode; 
  LIST_ENTRY        NodeModuleLink; 
  void*             LoadContext; 
  void*             ParentDllBase;
  void*             SwitchBackContext;
  RTLBalancedNode   BaseAddressIndexNode; 
  RTLBalancedNode   MappingInfoIndexNode; 
  uptr              OriginalBase; 
  LARGE_INTEGER     LoadTime; 
  u32               BaseNameHashValue; 
  LDRDllLoadReason  LoadReason; 
  u32               ImplicitPathOptions; 
  u32               ReferenceCount;

public:
  static LDRListEntry*  GetBase();
  static LoadOrderList* GetLoadOrder();
  static MemOrderList*  GetMemOrder();
  static InitOrderList* GetInitOrder();

  template <usize TableOffset = LDRListKind::memOrder>
  ALWAYS_INLINE TLDRListEntry<TableOffset>* asListEntry() const {
    using TblType = TLDRListEntry<TableOffset>;
    auto* const pNode = reinterpret_cast<TblType*>(asMutable());
    return pNode + TableOffset;
  }

  InitOrderList* inInitOrder() const;
  MemOrderList*  inMemOrder()  const;
  LoadOrderList* inLoadOrder() const;

  IMAGE_NT_HEADERS64* getNtHeader() const;

  ALWAYS_INLINE LDRDataTableEntry* asMutable() const {
    return const_cast<LDRDataTableEntry*>(this);
  }
};

template <usize TableOffset>
struct TLDRListEntry : public LDRListEntry {
  static_assert(TableOffset < 3);
  using BaseType = LDRListEntry;
  using SelfType = TLDRListEntry<TableOffset>;
  using TblType  = LDRDataTableEntry;

  struct iterator {
    using difference_type = ptrdiff;
    using value_type = TLDRListEntry*;
  public:
    bool operator==(const iterator&) const = default;
    value_type operator*()  const { return It; }
    value_type operator->() const { return It; }

    iterator& operator++() {
      this->It = It->prev();
      return *this;
    }
    iterator operator++(int) {
      iterator tmp = *this;
      ++*this;
      return tmp;
    }
  
  public:
    value_type It = nullptr;
  };

  struct ListProxy {
    iterator begin() const {
      return {end()->prev()};
    }
    iterator end() const {
      return {SelfType::GetSentinel()->asMutable()};
    }
  };

public:
  static SelfType* GetSentinel() {
    BaseType* Base = LDRDataTableEntry::GetBase();
    return static_cast<SelfType*>(Base + TableOffset);
  }

  inline SelfType* next() {
    auto* baseNext = this->BaseType::_iNext;
    return reinterpret_cast<SelfType*>(baseNext);
  }

  inline SelfType* prev() {
    auto* basePrev = this->BaseType::_iPrev;
    return reinterpret_cast<SelfType*>(basePrev);
  }

  UnicodeString name() const {
    return this->asDataTableEntry()->BaseDllName;
  }
  UnicodeString fullName() const {
    return this->asDataTableEntry()->FullDllName;
  }

  //==================================================================//
  // Conversions
  //==================================================================//

  ALWAYS_INLINE TblType* asDataTableEntry() const {
    SelfType* tableBase = asMutable() - TableOffset;
    return reinterpret_cast<TblType*>(tableBase);
  }

  ALWAYS_INLINE SelfType* asMutable() const {
    return const_cast<SelfType*>(this);
  }

  //==================================================================//
  // Misc.
  //==================================================================//

  static ListProxy Iterable() noexcept { return ListProxy{}; }
  iterator begin() const { return {end()->prev()}; }
  iterator end() const { return {SelfType::GetSentinel()->asMutable()}; }
};

extern template struct TLDRListEntry<0u>;
extern template struct TLDRListEntry<1u>;
extern template struct TLDRListEntry<2u>;

//////////////////////////////////////////////////////////////////////////
// Functions

extern "C" {

NTSYSAPI NTSTATUS NTAPI LdrEnumerateLoadedModules(
  PVOID                 Unused,
  LDRENUMPROC           Callback,
  PVOID                 Context);

NTSYSAPI NTSTATUS NTAPI LdrFindEntryForAddress(
  void*                 Address,
  LDRDataTableEntry**   TableEntry);

NTSYSAPI NTSTATUS NTAPI LdrGetDllHandle(
  u16*                  pwPath,
  void*                 Unused,
  UnicodeString*        ModuleFileName,
  HANDLE*               pHModule);

NTSYSAPI NTSTATUS NTAPI NtProtectVirtualMemory(
  HANDLE                ProcessHandle,
  void**                BaseAddress,
  usize*                RegionSize,
  u32                   NewProtection,
  u32*                  OldProtection);

NTSYSAPI void NTAPI     RtlGetNtVersionNumbers(
  u32*                  MajorVersion,
  u32*                  MinorVersion,
  u32*                  BuildNumber);

NTSYSAPI NTSTATUS NTAPI RtlAnsiStringToUnicodeString(
  UnicodeString*        DestinationString,
  const AnsiString*     SourceString,
  bool                  AllocateDestinationString);

NTSYSAPI void NTAPI     RtlInitAnsiString(
  AnsiString*           Dst,
  const char*           Src);

NTSYSAPI NTSTATUS NTAPI RtlQueryEnvironmentVariable(
  wchar_t*              Environment,
  const wchar_t*        Variable,
  usize                 VariableLength,
  wchar_t*              Buffer,
  usize                 BufferLength,
  usize*                ReturnLength);

NTSYSAPI NTSTATUS NTAPI RtlUnicodeStringToAnsiString(
  AnsiString*           DestinationString,
  const UnicodeString*  SourceString,
  bool                  AllocateDestinationString);

} // extern "C"

} // namespace re
