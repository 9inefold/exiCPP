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

#pragma once

#include <ArrayRef.hpp>
#include <Mem.hpp>
#include <OptInt.hpp>
#include <WinAPI.hpp>

namespace re {

enum DataDirectoryKind : u32 {
  DDK_ExportTable,
  DDK_ImportTable,
  DDK_ResourceTable,
  DDK_ExceptionTable,
  DDK_CertificateTable,
  DDK_BaseRelocationTable,
  DDK_Debug,
  DDK_Architecture,
  DDK_GlobalPtr,
  DDK_TLSTable,
  DDK_LoadConfigTable,
  DDK_BoundImport,
  DDK_ImportAddressTable,
  DDK_DelayImportDescriptor,
  DDK_CLRRuntimeHeader,
  DDK_Reserved,
};

template <DataDirectoryKind Kind>
struct DataDirectoryMap {
  using type = void;
};

#define DATA_DIRECTORY_MAP(kind, ty)            \
template <> struct DataDirectoryMap<DDK_##kind> \
{ using type = ty; };

DATA_DIRECTORY_MAP(ExportTable, IMAGE_EXPORT_DIRECTORY)
DATA_DIRECTORY_MAP(ImportTable, IMAGE_IMPORT_DESCRIPTOR)

template <DataDirectoryKind Kind>
using data_directory_t = typename DataDirectoryMap<Kind>::type;

//////////////////////////////////////////////////////////////////////////

struct RVAHandler;

class ExportHandler {
  friend struct RVAHandler;

  const RVAHandler* RVAs = nullptr;
  IMAGE_EXPORT_DIRECTORY* Exports = nullptr;

private:
  ExportHandler(
    const RVAHandler* RVAs,
    IMAGE_EXPORT_DIRECTORY* Exports) :
   RVAs(RVAs), Exports(Exports) {
  }

public:
  ExportHandler() = default;

  OptInt<u32> findNameIndex(const char* Str) const;
  void* getAddrFromIndex(u32 NameOffset) const;
  void* getExportRaw(const char* Str) const;

  void* operator[](const char* Str) const {
    return this->getExportRaw(Str);
  }

  template <typename T>
  T* getExport(const char* Str) const {
    return static_cast<T*>(
      this->getExportRaw(Str));
  }

  explicit operator bool() const {
    return Exports && (Exports->AddressOfFunctions != 0);
  }

private:
  ArrayRef<u32> getNameTable() const;
  ArrayRef<u16> getOrdinalTable() const;
  ArrayRef<u32> getAddrTable() const;
};

class ImportHandler {
  friend struct RVAHandler;

  const RVAHandler* RVAs = nullptr;
  MutArrayRef<IMAGE_IMPORT_DESCRIPTOR> Imports;

private:
  ImportHandler(
    const RVAHandler* RVAs,
    MutArrayRef<IMAGE_IMPORT_DESCRIPTOR> Imports) :
   RVAs(RVAs), Imports(Imports) {
  }

public:
  ImportHandler() = default;
  byte** findIATEntry(byte* Loc) const;
};


/// A wrapper around a module
struct RVAHandler {
  template <typename T>
  RVAHandler(T* Data, usize Size) :
   Base(ptr_cast<byte>(Data)), Size(Size) {
    static_assert(sizeof_v<T> <= 1);
    re_assert(checkCurrentModuleBase(),
      "Invalid base, not a DOS header!");
  }

  template <typename T> RVAHandler(T* Data) :
   RVAHandler(Data, sizeof(IMAGE_DOS_HEADER)) {
    this->updateSize();
  }

  byte* base() const { return Base; }
  usize size() const { return Size; }

public:
  /// Gets a pointer of type `T` from an RVA.
  template <typename T = void>
  T* get(usize Off) const {
    // Don't fuck up..
    re_assert((Off + sizeof_v<T>) <= this->Size);
    return aligned_ptr_cast<T>(Base + Off);
  }

  /// Gets an array of type `T` from an RVA & length.
  template <typename T>
  MutArrayRef<T> getArr(usize Off, usize Count) const {
    static_assert(sizeof_v<T> != 0, "Cannot use void!");
    re_assert((Off + Count * sizeof_v<T>) <= this->Size);
    return MutArrayRef<T>(get<T>(Off), Count);
  }

  /// Gets the offset of the default PE headers.
  usize getNtOffset() const;
  /// Gets the default PE headers.
  IMAGE_NT_HEADERS64* getNt() const;
  /// Gets the optional data-directories.
  ArrayRef<IMAGE_DATA_DIRECTORY> getOptDDs() const;

  /// Gets a specific data-directory.
  template <DataDirectoryKind Kind>
  inline data_directory_t<Kind>* getDD() const {
    void* Raw = this->getDDRaw(Kind);
    return ptr_cast<data_directory_t<Kind>>(Raw);
  }

  /// Gets an `ExportHandler` for the current module.
  ExportHandler exports() const {
    return ExportHandler(
      this, getDD<DDK_ExportTable>());
  }
  /// Gets an `ImportHandler` for the current module.
  ImportHandler imports() const;

  /// Gets an export from the current module.
  void* getExportRaw(const char* Str) const;
  /// Gets an export of type `T` from the current module.
  template <typename T>
  T* getExport(const char* Str) const {
    return static_cast<T*>(
      this->getExportRaw(Str));
  }

private:
  void updateSize();
  bool checkCurrentModuleBase() const;
  void* getDDRaw(DataDirectoryKind Kind) const;

private:
  byte* Base = nullptr;
  usize Size = 0;
};

} // namespace re
