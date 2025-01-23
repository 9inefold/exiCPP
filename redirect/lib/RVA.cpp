//===- RVA.hpp ------------------------------------------------------===//
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
/// This file implements a helper for dealing with
/// relative value addresses (RVAs).
///
//===----------------------------------------------------------------===//

#include <RVA.hpp>
#include <Logging.hpp>
#include <Strings.hpp>
#include "NtImports.hpp"

using namespace re;

//////////////////////////////////////////////////////////////////////////
// ExportHandler

ArrayRef<u32> ExportHandler::getNameTable() const {
  return RVAs->getArr<u32>(
    Exports->AddressOfNames,
    Exports->NumberOfNames);
}

ArrayRef<u16> ExportHandler::getOrdinalTable() const {
  return RVAs->getArr<u16>(
    Exports->AddressOfNameOrdinals,
    Exports->NumberOfNames);
}

ArrayRef<u32> ExportHandler::getAddrTable() const {
  return RVAs->getArr<u32>(
    Exports->AddressOfFunctions,
    Exports->NumberOfFunctions);
}

// ...

OptInt<u32> ExportHandler::findNameIndex(const char* Str) const {
  if UNLIKELY(!Str || Exports->NumberOfNames == 0)
    return re::nullopt;
  
  ArrayRef<u32> Names = getNameTable();
  for (i64 Ix = 0; Ix < Names.size(); ++Ix) {
    auto* Sym = RVAs->get<const char>(Names[Ix]);
    if (Strequal(Str, Sym))
      return static_cast<u32>(Ix);
  }

  return re::nullopt;
}

void* ExportHandler::getAddrFromIndex(u32 NameOffset) const {
  re_assert(NameOffset < Exports->NumberOfNames);
  auto Ords = getOrdinalTable();
  auto Addrs = getAddrTable();

  const u32 VAddr = Addrs[Ords[NameOffset]];
  return RVAs->get<>(VAddr);
}

void* ExportHandler::getExportRaw(const char* Str) const {
  if (!Str || *Str == '\0')
    return nullptr;
  if (OptInt<u32> Ix = findNameIndex(Str))
    return getAddrFromIndex(*Ix);
  return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// ImportHandler

byte** ImportHandler::findIATEntry(byte* Loc) const {
  if (Loc == nullptr)
    return nullptr;

  for (IMAGE_IMPORT_DESCRIPTOR& IID : Imports) {
    byte** IAT = RVAs->get<byte*>(IID.FirstThunk);
    for (; *IAT; ++IAT) {
      if (*IAT == Loc)
        return IAT;
    }
  }

  return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// RVAHandler

template <typename T>
static T* GetHead(const RVAHandler& RVAs) {
  const usize VBase = RVAs.get<IMAGE_DOS_HEADER>(0)->e_lfanew;
  return RVAs.get<T>(VBase);
}

static usize GetHeaderSize(RVAHandler& RVAs) {
  const usize VBase = RVAs.get<IMAGE_DOS_HEADER>(0)->e_lfanew;
  return VBase + sizeof(IMAGE_NT_HEADERS64);
}

bool RVAHandler::checkCurrentModuleBase() const {
  re_assert(Base != nullptr, "Modules cannot be null!");
  auto* Dos = get<IMAGE_DOS_HEADER>(0);
  return Dos->e_magic == 0x5A4D;
}

void RVAHandler::updateSize() {
  this->Size = GetHeaderSize(*this);
  auto* Nt = this->getNt();
  if LIKELY(Nt != nullptr) {
    this->Size = Nt->OptionalHeader.SizeOfImage;
  } else {
    // Should have the same offsets, but just to be sure...
    auto* Nt32 = GetHead<IMAGE_NT_HEADERS32>(*this);
    const u16 Magic = Nt32->OptionalHeader.Magic;
    re_assert(Magic == 0x10b);
    this->Size = Nt32->OptionalHeader.SizeOfImage;
  }
  // MiTrace("New size: %#zx", Size);
}

usize RVAHandler::getNtOffset() const {
  byte* Off = GetHead<byte>(*this);
  return (Off - this->Base);
}

IMAGE_NT_HEADERS64* RVAHandler::getNt() const {
  auto* Nt = GetHead<IMAGE_NT_HEADERS64>(*this);
  if UNLIKELY(Nt->Signature != 0x4550)
    return nullptr;
  if UNLIKELY(Nt->OptionalHeader.Magic != 0x20b)
    return nullptr;
  return Nt;
}

ArrayRef<IMAGE_DATA_DIRECTORY> RVAHandler::getOptDDs() const {
  auto* Nt = this->getNt();
  if (!Nt) return {nullptr, nullptr};
  auto& Opt = Nt->OptionalHeader;
  return {Opt.DataDirectory, Opt.NumberOfRvaAndSizes};
}

void* RVAHandler::getDDRaw(DataDirectoryKind Kind) const {
  if (ArrayRef DD = this->getOptDDs()) {
    if (DD.size() <= usize(Kind))
      return nullptr;
    auto [VirtualAddr, Size] = DD[Kind];
    if (!VirtualAddr)
      return nullptr;
    return get<>(VirtualAddr);
  }

  return nullptr;
}

ImportHandler RVAHandler::imports() const {
  auto* IID = getDD<DDK_ImportTable>();
  if (!IID) {
    return ImportHandler(
      this, {nullptr, nullptr});
  }

  auto* End = IID;
  while (End->OriginalFirstThunk && End->Name) {
    ++End;
  }
  return ImportHandler(this, {IID, End});
}

void* RVAHandler::getExportRaw(const char* Str) const {
  if UNLIKELY(!Str || *Str == '\0')
    return nullptr;
  if (ExportHandler E = exports())
    return E.getExportRaw(Str);
  return nullptr;
}
