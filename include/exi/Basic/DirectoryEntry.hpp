//===- exi/Basic/DirectoryEntry.hpp ----------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// Defines interfaces for DirectoryEntry and DirectoryEntryRef.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/DenseMapInfo.hpp>
#include <core/Common/Hashing.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/ErrorOr.hpp>
#include <utility>

namespace exi {

class FileManager;

namespace file_detail {
template <class RefT> class MapEntryOptionStorage;
} // namespace file_detail

// TODO: Implement Option<DirectoryEntryRef>

/// Cached information about one directory (either on disk or in the VFS).
class DirectoryEntry {
  friend class FileManager;
  DirectoryEntry() = default;
  DirectoryEntry(const DirectoryEntry&) = delete;
  DirectoryEntry& operator=(const DirectoryEntry&) = delete;
};

/// A reference to a `DirectoryEntry` that includes the name of the directory
/// as it was accessed by the FileManager's client. This information is embedded
/// by storing a `StringMapEntry`.
class DirectoryEntryRef {
public:
  const DirectoryEntry& getDirEntry() const { return *ME->getValue(); }
  StrRef getName() const { return ME->getKey(); }

  /// Hash code is based on the DirectoryEntry, not the specific named
  /// reference.
  friend hash_code hash_value(DirectoryEntryRef Ref) {
    return exi::hash_value(&Ref.getDirEntry());
  }

	/// The value type of the StringMap.
	using MapStore = ErrorOr<DirectoryEntry&>;
  using MapEntry = StringMapEntry<MapStore>;
  const MapEntry& getMapEntry() const { return *ME; }

  /// Check if RHS referenced the file in exactly the same way.
  bool isSameRef(DirectoryEntryRef RHS) const { return ME == RHS.ME; }

  DirectoryEntryRef() = delete;
  explicit DirectoryEntryRef(const MapEntry &ME) : ME(&ME) {}

private:
	friend class file_detail::MapEntryOptionStorage<DirectoryEntryRef>;
	struct optional_none_tag {};

	// Private constructor for use by OptionStorage.
  DirectoryEntryRef(optional_none_tag) : ME(nullptr) {}
  bool hasOptionalValue() const { return ME; }

  friend struct DenseMapInfo<DirectoryEntryRef>;
  struct dense_map_empty_tag {};
  struct dense_map_tombstone_tag {};

  // Private constructors for use by DenseMapInfo.
  DirectoryEntryRef(dense_map_empty_tag)
      : ME(DenseMapInfo<const MapEntry*>::getEmptyKey()) {}
  DirectoryEntryRef(dense_map_tombstone_tag)
      : ME(DenseMapInfo<const MapEntry*>::getTombstoneKey()) {}
  bool isSpecialDenseMapKey() const {
    return isSameRef(DirectoryEntryRef(dense_map_empty_tag())) ||
           isSameRef(DirectoryEntryRef(dense_map_tombstone_tag()));
  }

  const MapEntry* ME;
};

//////////////////////////////////////////////////////////////////////////
// Option Specialization

using OptionalDirectoryEntryRef = Option<DirectoryEntryRef>;

namespace file_detail {

/// Customized storage for refs derived from map entires in FileManager, using
/// the private optional_none_tag to keep it to the size of a single pointer.
template <class RefT> class MapEntryOptionStorage {
  using optional_none_tag = typename RefT::optional_none_tag;
  RefT MaybeRef;
public:
  MapEntryOptionStorage() : MaybeRef(optional_none_tag()) {}
  explicit MapEntryOptionStorage(std::in_place_t, auto&&...Args)
    : MaybeRef(EXI_FWD(Args)...) {}

  void reset() { MaybeRef = optional_none_tag(); }

  bool has_value() const { return MaybeRef.hasOptionalValue(); }

  RefT& value() & {
    exi_assert(has_value());
    return MaybeRef;
  }
  const RefT& value() const& {
    exi_assert(has_value());
    return MaybeRef;
  }
  RefT&& value() && {
    exi_assert(has_value());
    return std::move(MaybeRef);
  }

  void emplace(auto&&...Args) {
    MaybeRef = RefT(EXI_FWD(Args)...);
  }

  MapEntryOptionStorage& operator=(RefT Ref) {
    MaybeRef = Ref;
    return *this;
  }
};

} // namespace file_detail

/// Customize OptionalStorage<DirectoryEntryRef> to use DirectoryEntryRef and
/// its optional_none_tag to keep it the size of a single pointer.
template <> class OptionStorage<DirectoryEntryRef>
    : public file_detail::MapEntryOptionStorage<DirectoryEntryRef> {
  using StorageImpl =
      file_detail::MapEntryOptionStorage<DirectoryEntryRef>;
public:
  OptionStorage() = default;

  explicit OptionStorage(std::in_place_t, auto&&...Args)
      : StorageImpl(std::in_place_t{}, EXI_FWD(Args)...) {}

  OptionStorage &operator=(DirectoryEntryRef Ref) {
    StorageImpl::operator=(Ref);
    return *this;
  }
};

static_assert(sizeof(OptionalDirectoryEntryRef) == sizeof(DirectoryEntryRef),
              "OptionalDirectoryEntryRef must avoid size overhead");

static_assert(std::is_trivially_copyable_v<OptionalDirectoryEntryRef>,
              "OptionalDirectoryEntryRef should be trivially copyable");

//////////////////////////////////////////////////////////////////////////

template <> struct PointerLikeTypeTraits<DirectoryEntryRef> {
  static inline void* getAsVoidPointer(DirectoryEntryRef Dir) {
    return const_cast<DirectoryEntryRef::MapEntry*>(&Dir.getMapEntry());
  }

  static inline DirectoryEntryRef getFromVoidPointer(void* Ptr) {
    return DirectoryEntryRef(
        *reinterpret_cast<const DirectoryEntryRef::MapEntry*>(Ptr));
  }

  static constexpr int NumLowBitsAvailable = PointerLikeTypeTraits<
      const DirectoryEntryRef::MapEntry*>::NumLowBitsAvailable;
};

/// Specialisation of DenseMapInfo for DirectoryEntryRef.
template <> struct DenseMapInfo<DirectoryEntryRef> {
  static inline DirectoryEntryRef getEmptyKey() {
    return DirectoryEntryRef(
      DirectoryEntryRef::dense_map_empty_tag());
  }

  static inline DirectoryEntryRef getTombstoneKey() {
    return DirectoryEntryRef(
      DirectoryEntryRef::dense_map_tombstone_tag());
  }

  static unsigned getHashValue(DirectoryEntryRef Val) {
    return hash_value(Val);
  }

  static bool isEqual(DirectoryEntryRef LHS,
                      DirectoryEntryRef RHS) {
    // Catch the easy cases: both empty, both tombstone, or the same ref.
    if (LHS.isSameRef(RHS))
      return true;

    // Confirm LHS and RHS are valid.
    if (LHS.isSpecialDenseMapKey() || RHS.isSpecialDenseMapKey())
      return false;

    // It's safe to compare values.
    return &LHS.getDirEntry() == &RHS.getDirEntry();
  }
};

} // namespace exi
