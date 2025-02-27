//===- exi/Basic/FileEntry.hpp --------------------------------------===//
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
/// Defines interfaces for FileEntry and FileEntryRef.
///
//===----------------------------------------------------------------===//

#include <core/Common/Box.hpp>
#include <core/Common/DenseMapInfo.hpp>
#include <core/Common/Hashing.hpp>
#include <core/Common/PointerUnion.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/ErrorOr.hpp>
#include <core/Support/MemoryBufferRef.hpp>
#include <core/Support/FileSystem/UniqueID.hpp>
#include <exi/Basic/DirectoryEntry.hpp>

namespace exi {

class MemoryBuffer;
class WritableMemoryBuffer;
class FileManager;

namespace vfs {
class File;
} // namespace vfs

/// Cached information about a file (either on disk or in the VFS).
class alignas(8) FileEntry {
  friend class FileManager;
  FileEntry();
  FileEntry(const FileEntry&) = delete;
  FileEntry& operator=(const FileEntry&) = delete;

  /// "Real" path of the file, may not exist.
  String ExternalName;
  /// The size of the original file.
  off_t Size = 0;
  /// The directory the file resides in.
  const DirectoryEntry* Dir = nullptr;
  /// The file's unique identifier.
  sys::fs::UniqueID UniqueID;
  bool IsNamedPipe = false;

  /// The open file, if owned by the cache.
  mutable Box<vfs::File> TheFile;
  /// The actual buffer containing the input.
  mutable Box<MemoryBuffer> TheBuffer;

public:
  /// If the buffer was a `WritableMemoryBuffer`.
  EXI_PREFER_TYPE(bool)
  unsigned IsMutable : 1;

  /// If the file may change between stat invocations.
  EXI_PREFER_TYPE(bool)
  unsigned IsVolatile : 1;

  /// If the buffer original contents were overridden.
  EXI_PREFER_TYPE(bool)
  unsigned BufferOverridden : 1;

  /// If the buffer content has changed, but the buffer remains the same.
  EXI_PREFER_TYPE(bool)
  mutable unsigned IsDirty : 1;

public:
  ~FileEntry();

  StrRef getFilename() const { return ExternalName; }
  off_t getSize() const { return Size; }
  /// Size may change due to a UTF conversion.
  void setSize(off_t NewSize) { Size = NewSize; }
  const sys::fs::UniqueID& getUniqueID() const { return UniqueID; }

  /// Get the directory the file resides in.
  const DirectoryEntry* getDir() const { return Dir; }

  /// Check whether the file is a named pipe (and thus can't be opened by
  /// the native FileManager methods).
  bool isNamedPipe() const { return IsNamedPipe; }

  /// Close the VFS file, if it exists.
  void closeFile() const;

  /// Return the buffer, if loaded.
  Option<MemoryBufferRef> getBufferIfLoaded() const;

  /// Return a mutable reference to the buffer, if loaded.
  Option<WritableMemoryBuffer&> getWriteBufferIfLoaded() const;

  void setBuffer(Box<MemoryBuffer> Buffer);
  void setBuffer(Box<WritableMemoryBuffer> Buffer);

private:
  WritableMemoryBuffer* getMutBuffer() const;
};

/// A reference to a `FileEntry` that includes the name of the file
/// as it was accessed by the FileManager's client.
class FileEntryRef {
public:
  /// The name of this FileEntry, as originally requested without applying any
  /// remappings for VFS 'use-external-name'.
  StrRef getName() const { return ME->first(); }

  /// The name of this FileEntry. If a VFS uses 'use-external-name', this is
  /// the redirected name. See getName().
  StrRef getNameFromBase() const { return getBaseMapEntry().first(); }

  const FileEntry& getFileEntry() const {
    return *cast<FileEntry*>(getBaseMapEntry().second->V);
  }

  /// This function is used if the buffer size needs to be updated
  /// due to potential UTF conversions.
  void updateFileEntryBufferSize(unsigned BufferSize) {
    cast<FileEntry*>(getBaseMapEntry().second->V)->setSize(BufferSize);
  }

  DirectoryEntryRef getDir() const { return ME->second->Dir; }

  inline off_t getSize() const;
  inline const sys::fs::UniqueID& getUniqueID() const;
  inline void closeFile() const;

  /// Check if the underlying FileEntry is the same, intentially ignoring
  /// whether the file was referenced with the same spelling of the filename.
  friend bool operator==(const FileEntryRef& LHS, const FileEntryRef& RHS) {
    return &LHS.getFileEntry() == &RHS.getFileEntry();
  }
  friend bool operator==(const FileEntry* LHS, const FileEntryRef& RHS) {
    return LHS == &RHS.getFileEntry();
  }
  friend bool operator==(const FileEntryRef& LHS, const FileEntry* RHS) {
    return &LHS.getFileEntry() == RHS;
  }
  friend bool operator!=(const FileEntryRef& LHS, const FileEntryRef& RHS) {
    return !(LHS == RHS);
  }
  friend bool operator!=(const FileEntry* LHS, const FileEntryRef& RHS) {
    return !(LHS == RHS);
  }
  friend bool operator!=(const FileEntryRef& LHS, const FileEntry* RHS) {
    return !(LHS == RHS);
  }

  /// Hash code is based on the FileEntry, not the specific named reference,
  /// just like operator==.
  friend hash_code hash_value(FileEntryRef Ref) {
    return hash_value(&Ref.getFileEntry());
  }

  struct MapValue;

  /// Type used in the StringMap.
  using MapEntry = StringMapEntry<ErrorOr<MapValue>>;

  /// Type stored in the StringMap.
  struct MapValue {
    /// The pointer at another MapEntry is used when the FileManager should
    /// silently forward from one name to another, which occurs in Redirecting
    /// VFSs that use external names. In that case, the \c FileEntryRef
    /// returned by the \c FileManager will have the external name, and not the
    /// name that was used to lookup the file.
    PointerUnion<FileEntry*, const MapEntry*> V;

    /// Directory the file was found in.
    DirectoryEntryRef Dir;

    MapValue() = delete;
    MapValue(FileEntry& FE, DirectoryEntryRef Dir) : V(&FE), Dir(Dir) {}
    MapValue(MapEntry& ME, DirectoryEntryRef Dir) : V(&ME), Dir(Dir) {}
  };

  /// Check if RHS referenced the file in exactly the same way.
  bool isSameRef(const FileEntryRef& RHS) const { return ME == RHS.ME; }

  FileEntryRef() = delete;
  explicit FileEntryRef(const MapEntry& ME) : ME(&ME) {
    exi_assert(ME.second, "Expected payload");
    exi_assert(ME.second->V, "Expected non-null");
  }

  /// Expose the underlying MapEntry to simplify packing in a PointerIntPair or
  /// PointerUnion and allow construction in Optional.
  const FileEntryRef::MapEntry& getMapEntry() const { return *ME; }

  /// Retrieve the base MapEntry after redirects.
  const MapEntry& getBaseMapEntry() const {
    const MapEntry* Base = ME;
    while (const auto* Next
        = dyn_cast<const MapEntry*>(Base->second->V)) {
      Base = Next;
    }
    return *Base;
  }

private:
  friend class file_detail::MapEntryOptionStorage<FileEntryRef>;
  struct optional_none_tag {};

  // Private constructor for use by OptionStorage.
  FileEntryRef(optional_none_tag) : ME(nullptr) {}
  bool hasOptionalValue() const { return ME; }

  friend struct DenseMapInfo<FileEntryRef>;
  struct dense_map_empty_tag {};
  struct dense_map_tombstone_tag {};

  // Private constructors for use by DenseMapInfo.
  FileEntryRef(dense_map_empty_tag)
      : ME(DenseMapInfo<const MapEntry*>::getEmptyKey()) {}
  FileEntryRef(dense_map_tombstone_tag)
      : ME(DenseMapInfo<const MapEntry*>::getTombstoneKey()) {}
  bool isSpecialDenseMapKey() const {
    return isSameRef(FileEntryRef(dense_map_empty_tag())) ||
           isSameRef(FileEntryRef(dense_map_tombstone_tag()));
  }

  const MapEntry* ME;
};

static_assert(sizeof(FileEntryRef) == sizeof(const FileEntry*),
             "FileEntryRef must avoid size overhead");

//////////////////////////////////////////////////////////////////////////
// Option Specialization

using OptionalFileEntryRef = Option<FileEntryRef>;

/// Customize OptionalStorage<DirectoryEntryRef> to use DirectoryEntryRef and
/// its optional_none_tag to keep it the size of a single pointer.
template <> class OptionStorage<FileEntryRef>
    : public file_detail::MapEntryOptionStorage<FileEntryRef> {
  using StorageImpl =
      file_detail::MapEntryOptionStorage<FileEntryRef>;
public:
  OptionStorage() = default;

  explicit OptionStorage(std::in_place_t, auto&&...Args)
      : StorageImpl(std::in_place_t{}, EXI_FWD(Args)...) {}

  OptionStorage& operator=(FileEntryRef Ref) {
    StorageImpl::operator=(Ref);
    return *this;
  }
};

static_assert(sizeof(OptionalFileEntryRef) == sizeof(FileEntryRef),
              "OptionalFileEntryRef must avoid size overhead");

static_assert(std::is_trivially_copyable_v<OptionalFileEntryRef>,
              "OptionalFileEntryRef should be trivially copyable");

//////////////////////////////////////////////////////////////////////////

/// Specialisation of DenseMapInfo for FileEntryRef.
template <> struct DenseMapInfo<FileEntryRef> {
  static inline FileEntryRef getEmptyKey() {
    return FileEntryRef(FileEntryRef::dense_map_empty_tag());
  }

  static inline FileEntryRef getTombstoneKey() {
    return FileEntryRef(FileEntryRef::dense_map_tombstone_tag());
  }

  static unsigned getHashValue(FileEntryRef Val) {
    return hash_value(Val);
  }

  static bool isEqual(FileEntryRef LHS, FileEntryRef RHS) {
    // Catch the easy cases: both empty, both tombstone, or the same ref.
    if (LHS.isSameRef(RHS))
      return true;

    // Confirm LHS and RHS are valid.
    if (LHS.isSpecialDenseMapKey() || RHS.isSpecialDenseMapKey())
      return false;

    // It's safe to use operator==.
    return LHS == RHS;
  }

  /// Support for finding `const FileEntry*` in a `DenseMap<FileEntryRef, T>`.
  /// @{
  static unsigned getHashValue(const FileEntry* Val) {
    return hash_value(Val);
  }
  static bool isEqual(const FileEntry* LHS, FileEntryRef RHS) {
    if (RHS.isSpecialDenseMapKey())
      return false;
    return LHS == RHS;
  }
  /// @}
};

off_t FileEntryRef::getSize() const { return getFileEntry().getSize(); }

const sys::fs::UniqueID& FileEntryRef::getUniqueID() const {
  return getFileEntry().getUniqueID();
}

void FileEntryRef::closeFile() const { getFileEntry().closeFile(); }

} // namespace exi
