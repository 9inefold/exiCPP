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
/// String tables have no understanding of the EXI format (other than length),
/// they simply cache the provided values.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/PagedVec.hpp>
#include <core/Common/SmallLRUCache.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/TinyPtrVec.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/StringSaver.hpp>
#include <exi/Basic/CompactID.hpp>
#include <exi/Basic/EventCodes.hpp>

namespace exi {

struct ExiOptions;

//===----------------------------------------------------------------===//
// Decoding
//===----------------------------------------------------------------===//

/// Defines utilities for decoding EXI.
namespace decode {

class StringTable;

/// For single associations.
using IDPair = std::pair<StrRef, CompactID>;

/// For double associations in Global/LocalValue additions.
struct IDTriple {
  StrRef Value;
  CompactID GlobalID = 0;
  CompactID LocalID = 0;
};

/// The value stored for each entry in the URI map.
struct URIInfo {
  StrRef Name; /// Data for [namespace]:local-name
  u32 PrefixElts = 1; /// Number of elements in Prefix partition.
  u32 LNElts = 0; /// Number of elements in LocalName partition.
};

/// The value stored for each entry in the LocalName map.
struct LocalName {
  using value_type = SmallVec<InlineStr*, 2>;
  StrRef Name; /// namespace:[local-name]
  InlineStr* FullName = nullptr; /// [namespace:local-name]
  value_type LocalValues;
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

/// The string table used for decoding.
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
  CompactIDCounter<1> URICount;

  /// Maps a URI to a (likely) singular value.
  using PrefixMapType = SmallVec<TinyPtrVec<InlineStr*>, kSchemaElts>;
  /// Used to map URI indices to Prefixes, where there is likely only one.
  /// If Prefixes are preserved, this mapping will be enabled.
  /// If a given Prefix partition has <= 1 elements, it is omitted.
  /// FIXME: This should maybe be a tagged union instead...
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
  CompactIDCounter<> LNCount;

  using LNPartition = LocalName::value_type;
  /// Caches a mapping from a QName to a LocalName.
  using LNCacheType = SmallLRUCache<SmallQName, LNPartition*, 4>;
  /// Used to cache recently used values. Since you generally have repetitive
  /// lookups, this may slightly increase performance, as it saves lookups.
  /// TODO: Profile!!
  mutable LNCacheType LNCache;

  /// Used to map LocalName IDs to GlobalValues.
  ///  Eg. `GValueMap[GlobalID]`
  SmallVec<InlineStr*, 0> GValueMap;
  CompactIDCounter<> GValueCount;

  bool DidSetup : 1 = false;
  /// If the tables should wrap once reaching their capacity.
  bool WrappingValues : 1 = false;

public:
  StringTable();
  StringTable(const ExiOptions& Opts) : StringTable() {
    this->setup(Opts);
  }

  /// Sets up the initial decoder state.
  /// The signature will have to change when schemas are introduced.
  void setup(const ExiOptions& Opts);

  /// Gets an `InlineStr` from an interned `StrRef`.
  [[nodiscard]] const InlineStr* getInline(StrRef Str) const {
    const char* RawStr = (Str.data() - offsetof(InlineStr, Data));
    exi_invariant(NameValueCache.getAllocator().identifyObject(RawStr));
    auto* const InlStr = reinterpret_cast<const InlineStr*>(RawStr);
    exi_assert(InlStr->Size == Str.size());
    return std::launder(InlStr);
  }

  ////////////////////////////////////////////////////////////////////////
  // Setters

  /// Creates a new URI.
  IDPair addURI(StrRef URI, Option<StrRef> Pfx = std::nullopt);
  /// Associates a new Prefix with a URI.
  IDPair addPrefix(CompactID URI, StrRef Pfx);
  /// Associates a new LocalName with a URI.
  IDPair addLocalName(CompactID URI, StrRef Name);

  /// Creates a new GlobalValue.
  IDPair addGlobalValue(StrRef Value);
  /// Associates a new LocalValue with a (URI, LocalNameID).
  inline IDPair addLocalValue(CompactID URI, CompactID LocalID, StrRef Value) {
    return this->addLocalValue(SmallQName::NewQName(URI, LocalID), Value);
  }
  /// Associates a new LocalValue with a QName.
  IDPair addLocalValue(SmallQName IDs, StrRef Value) {
    exi_invariant(IDs.isQName());

    LNPartition& Values = *getLVPartition(IDs);
    const CompactID ID = Values.size();
    // Add to the global table.
    InlineStr* Str = createGlobalValue(Value);
    // Add to the local table for URI:LocalID.
    Values.push_back(Str);

    return {Str->str(), ID};
  }

  /// Creates a new GlobalValue AND associates a new LocalValue with QName.
  inline IDTriple addValue(CompactID URI, CompactID LocalID, StrRef Value) {
    return this->addValue(SmallQName::NewQName(URI, LocalID), Value);
  }
  /// Creates a new GlobalValue AND associates a new LocalValue with QName.
  IDTriple addValue(SmallQName IDs, StrRef Value) {
    exi_invariant(IDs.isQName());
    // auto [Str, GID] = this->addGlobalValue(Value);
    auto [Str, LnID] = this->addLocalValue(IDs, Value);
    const CompactID GID = (*GValueCount - 1);
    return {.Value = Str, .GlobalID = GID, .LocalID = LnID};
  }

  ////////////////////////////////////////////////////////////////////////
  // Validators

  bool hasURI(CompactID URI) const {
    return URI < URIMap.size();
  }

  /// Checks if URI has prefixes.
  bool hasPrefix(CompactID URI) const {
    if EXI_UNLIKELY(!this->hasURI(URI))
      return false;
    this->assertPartitionsInSync();
    return URIMap[URI].PrefixElts > 0;
  }

  /// Checks if URI has a prefix.
  bool hasPrefix(CompactID URI, CompactID PfxID) const {
    if EXI_UNLIKELY(!this->hasPrefix(URI))
      return false;
    return PfxID < URIMap[URI].PrefixElts;
  }

  ////////////////////////////////////////////////////////////////////////
  // Getters

  /// Gets a URI from an ID.
  StrRef getURI(CompactID URI) const {
    exi_invariant(URI < URIMap.size());
    this->assertPartitionsInSync();
    return URIMap[URI].Name;
  }

  /// Gets a URI from an ID.
  StrRef getPrefix(CompactID URI, CompactID PfxID) const {
    exi_assert(this->hasPrefix(URI));
    auto& Pfx = PrefixMap[URI];
    exi_invariant(PfxID < Pfx.size());
    return Pfx[PfxID]->str();
  }

  /// Gets a LocalName from a (URI, LocalID).
  StrRef getLocalName(CompactID URI, CompactID LocalID) const {
    exi_invariant(URI < URIMap.size());
    exi_invariant(LocalID < URIMap[URI].LNElts);
    this->assertPartitionsInSync();
    return LNMap[URI][LocalID]->Name;
  }

  /// Gets a LocalName from a [URI, LocalID].
  StrRef getLocalName(SmallQName IDs) const {
    exi_assert(IDs.isQName());
    return getLocalName(IDs.URI, IDs.LocalID);
  }

  /// Gets a [URI, LocalName] from a [URI, LocalID].
  std::pair<StrRef, StrRef> getQName(CompactID URI, CompactID LocalID) const {
    exi_invariant(URI < URIMap.size());
    exi_invariant(LocalID < URIMap[URI].LNElts);
    this->assertPartitionsInSync();
    
    StrRef Name = URIMap[URI].Name;
    StrRef LocalName = LNMap[URI][LocalID]->Name;
    return {Name, LocalName};
  }

  /// Gets a [URI, LocalName] from a [URI, LocalID].
  std::pair<StrRef, StrRef> getQName(SmallQName IDs) const {
    return getQName(IDs.URI, IDs.LocalID);
  }

  /// Gets a GlobalValue from an ID.
  StrRef getGlobalValue(CompactID GlobalID) const {
    exi_invariant(GlobalID < *GValueCount);
    return GValueMap[GlobalID]->str();
  }

  /// Gets a LocalValue from a (URI, LocalID, ValueID).
  StrRef getLocalValue(CompactID URI, CompactID LocalID, CompactID ValueID) const {
    return this->getLocalValue(SmallQName::NewQName(URI, LocalID), ValueID);
  }

  /// Gets a LocalValue from a ([URI, LocalID], ValueID).
  StrRef getLocalValue(SmallQName IDs, CompactID ValueID) const {
    exi_assert(IDs.isQName());
    const LNPartition& Values = *getLVPartition(IDs);
    exi_invariant(ValueID < Values.size());
    return Values[ValueID]->str();
  }

  /// Gets a Local or Global Value from a ([URI, LocalID]?, ValueID).
  StrRef getValue(EventUID IDs) const {
    exi_relassert(IDs.hasValue());
    if (IDs.isGlobal())
      return getGlobalValue(IDs.ValueID);
    else
      // Use this overload for implicit QName validity checks.
      return getLocalValue(IDs.Name, IDs.ValueID);
  }

  ////////////////////////////////////////////////////////////////////////
  // Log Getters

  EXI_INLINE u64 getURILog() const {
    return URICount.bits();
  }

  /// Gets the bit number for QName prefixes.
  u64 getPrefixLogQ(CompactID URI) const {
    exi_invariant(URI < URIMap.size());
    this->assertPartitionsInSync();
    const u64 Count = URIMap[URI].PrefixElts;
    if EXI_UNLIKELY(Count == 0)
      return 0;
    return CompactIDLog2(Count - 1);
  }

  /// Gets the bit number for QName prefixes.
  u64 getPrefixLog(CompactID URI) const {
    exi_invariant(URI < URIMap.size());
    this->assertPartitionsInSync();
    return CompactIDLog2(URIMap[URI].PrefixElts);
  }

  u64 getLocalNameLog(CompactID URI) const {
    exi_invariant(URI < URIMap.size());
    this->assertPartitionsInSync();
    return CompactIDLog2(URIMap[URI].LNElts);
  }

  EXI_INLINE u64 getGlobalValueLog() const {
    return GValueCount.bits();
  }

  EXI_INLINE u64 getLocalValueLog(CompactID URI, CompactID LocalID) const {
    return this->getLocalValueLog(SmallQName::NewQName(URI, LocalID));
  }

  u64 getLocalValueLog(SmallQName IDs) const {
    exi_assert(IDs.isQName());
    const LNPartition* Values = getLVPartition(IDs);
    return CompactIDLog2(Values->size());
  }

private:
  [[nodiscard]] InlineStr* intern(StrRef Str) {
    return NameValueCache.saveRaw(Str);
  }
  [[nodiscard]] StrRef internStr(StrRef Str) {
    return NameValueCache.save(Str);
  }

  [[nodiscard]] LNPartition* getLVPartition(SmallQName IDs) {
    // Our LRU policy currently prohibits null keys.
    // TODO: Handle these cases?
    LNPartition*& Partition = *LNCache.get(IDs);
    if (Partition != nullptr)
      return Partition;
    
    const u64 URI = IDs.URI, LocalID = IDs.LocalID;
    exi_invariant(URI < URIMap.size());
    exi_invariant(LocalID < URIMap[URI].LNElts);
    this->assertPartitionsInSync();

    // Set the value of the cached partition.
    LocalName* LN = LNMap[URI][LocalID];
    return (Partition = &LN->LocalValues);
  }

  [[nodiscard]] const LNPartition* getLVPartition(SmallQName IDs) const {
    return const_cast<StringTable*>(this)->getLVPartition(IDs);
  }

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
  [[nodiscard]] LocalName* createLocalName(StrRef Name) {
    StrRef Str = internStr(Name);
    LocalName* Ptr = LNAllocator.Allocate();
    return new (Ptr) LocalName {
      .Name = Str, .LocalValues = {}
    };
  }

  /// Gets a new global value (which is added to the global partition).
  [[nodiscard]] InlineStr* createGlobalValue(StrRef Value) {
    InlineStr* Str = intern(Value);
    exi_invariant(Str, "Invalid allocation??");
    GValueMap.push_back(Str);
    ++GValueCount;
    return Str;
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

/// TODO: The string table used for encoding.
class StringTable {
  /// The allocator shared internally.
  exi::BumpPtrAllocator Alloc;
  /// Used to unique strings for lookup.
  exi::UniqueStringSaver NameCache;

  // TODO: Finish design...
};

} // namespace encode

} // namespace exi
