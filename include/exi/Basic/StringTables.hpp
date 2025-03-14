//===- exi/Basic/StringTables.hpp -----------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines the various tables used by the EXI processor.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/PagedVec.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/TinyPtrVec.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/StringSaver.hpp>
#include <exi/Basic/CompactID.hpp>

namespace exi {

struct ExiOptions;

//===----------------------------------------------------------------===//
// Decoding
//===----------------------------------------------------------------===//

/// Defines utilities for decoding EXI.
namespace decode {

class StringTable;

/// The value stored for each entry in the URI map.
struct URIInfo {
  StrRef Name; /// Data for [namespace]:local-name
  u32 PrefixElts = 1; /// Number of elements in Prefix partition.
  u32 LNElts = 0; /// Number of elements in LocalName partition.
};

/// The value stored for each entry in the LocalName map.
struct LocalName {
  StrRef Name; /// namespace:[local-name]
  InlineStr* FullName = nullptr; /// [namespace:local-name]
  SmallVec<InlineStr*, 0> LocalValues;
public:
  /// Returns the minimum bits required for current amount of local values.
  u32 bits() const {
    // exi_invariant(not LocalValues.empty());
    return CompactIDLog2(LocalValues.size() + 1);
  }
  /// Returns the minimum bytes required for current amount of local values.
  u32 bytes() const {
    if EXI_UNLIKELY(LocalValues.empty())
      return 0;
    return (bits() / 8) + 1u;
  }
};

class StringTable {
  /// Allocator used by `LNMap`.
  mutable exi::BumpPtrAllocator LNPageAllocator;
  /// Allocator used for LocalNames.
  exi::SpecificBumpPtrAllocator<LocalName> LNAllocator;
  /// Used to unique strings for output.
  exi::OwningStringSaver NameValueCache;

  /// Small size for schema adjacent values.
  static constexpr usize kSchemaElts = 4;

  /// Used to map URI indices to strings.
  SmallVec<URIInfo, kSchemaElts> URIMap;
  CompactIDCounter URICount;

  /// Maps a URI to a (likely) singular value.
  using PrefixMapType = SmallVec<TinyPtrVec<InlineStr*>, kSchemaElts>;
  /// Used to map URI indices to prefixes, where there is likely only one.
  /// If prefixes are preserved, this mapping will be enabled.
  /// FIXME: This should probably be a tagged union instead...
  PrefixMapType PrefixMap;

  /// Small size for schema adjacent values.
  static constexpr usize kLNPageElts = 32;

  /// Maps an ID to a LocalName.
  using LNMapType = SmallVec<LocalName*, 0>;
  /// Used to map URI indices to LocalNames. Using `PagedVec` for stable
  /// pointers, and I may introduce an LRU cache in the future.
  ///  Eg. `LNMap[URI][LocalID]->LocalValues[ValueID]`
  /// TODO: Cache?? And maybe use a deque instead...
  PagedVec<LNMapType, kLNPageElts> LNMap;
  CompactIDCounter LNCount;

  /// Used to map LocalName IDs to GlobalValues.
  ///  Eg. `GValueMap[GlobalID]`
  SmallVec<InlineStr*, 0> GValueMap;
  CompactIDCounter GValueCount;

  bool DidSetup = false;

public:
  StringTable();
  StringTable(const ExiOptions& Opts) : StringTable() {
    this->setup(Opts);
  }

  /// Sets up the initial decoder state.
  /// FIXME: The signature will have to change when schemas are introduced.
  void setup(const ExiOptions& Opts);

private:
  InlineStr* intern(StrRef Str) { return NameValueCache.saveRaw(Str); }
  StrRef internStr(StrRef Str) { return NameValueCache.save(Str); }

  /// Checks if partitions are of equal size.
  EXI_INLINE void assertPartitionsInSync() const {
    exi_invariant(URIMap.size() == PrefixMap.size(),
                "URI and Prefix partitions out of sync!");
    exi_invariant(URIMap.size() == *LNCount,
                  "URI and LocalName partitions out of sync!");
  }

  /// Gets a new (Info, ID) pair from a URI and Prefix.
  std::pair<URIInfo*, CompactID>
   createURI(StrRef URI, Option<StrRef> Pfx = std::nullopt);

  /// Gets a new LocalName.
  LocalName* createLocalName(StrRef Name) {
    StrRef S = internStr(Name);
    LocalName* Ptr = LNAllocator.Allocate();
    return new (Ptr) LocalName { .Name = S };
  }

  /// Creates the initial entries for the string table. The values inserted
  /// depend on the schema.
  void createInitialEntries(bool UsesSchema);

  /// Appends LocalNames to the provided URI.
  void appendLocalNames(CompactID ID, ArrayRef<StrRef> LocalNames);
};

} // namespace decode

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

/// Defines utilities for encoding EXI.
namespace encode {

class StringTable;

class StringTable {
  /// The allocator shared internally.
  exi::BumpPtrAllocator Alloc;
  /// Used to unique strings for lookup.
  exi::UniqueStringSaver NameCache;

  // TODO: Finish design...
};

} // namespace encode

} // namespace exi
