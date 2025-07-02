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

#include <core/Common/CachedHashString.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/Naked.hpp>
#include <core/Common/OpaqueHandle.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/MathExtras.hpp>
#include <core/Support/StringSaver.hpp>
#include <exi/Basic/CompactID.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <type_traits>

// TODO: Refactor to use embedded counters with RTTI handles

/// If the cache should be LRU (a single pair otherwise).
#define EXI_ENCODE_URISTACK_CACHE 0

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
  /// TODO: Profile!! Four is good for now, but check various values.
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

  /// Gets a Prefix from a URI.
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

template <typename Value, bool IsOwned = false>
using BumpStringMap = StringMap<Value,
  std::conditional_t<IsOwned, BumpPtrAllocator, BumpPtrAllocator&>>;

// TODO: Enable macro expansion for EXI_OPAQUE_HANDLE
// See https://stackoverflow.com/questions/42300539/documenting-macros-using-doxygen

/// @typedef STPrefixEntry
/// Typed handle for `StringTable::PrefixEntry`.
EXI_OPAQUE_HANDLE(STPrefixEntry, StringTable);
/// @typedef STURIEntry
/// Typed handle for `StringTable::URIEntry`.
EXI_OPAQUE_HANDLE(STURIEntry, StringTable);
//struct STURIEntry;
/// @typedef STValueEntry
/// Typed handle for `StringTable::ValueEntry`.
EXI_OPAQUE_HANDLE(STValueEntry, StringTable);
/// @typedef QualName
/// Handle for an `InlineString` representing a QName's data as `"URI$ln"`.
EXI_OPAQUE_HANDLE(QualName, StringTable);

/// Using `u32`, because if you have 4 billion uris... wtf.
struct PrefixInfo {
  STURIEntry* Link = nullptr;
  /// The ID of the URI.
  u32 WithURI = 0;
  /// The ID of the prefix.
  u16 Pfx = 0;
  /// The cached prefix log.
  u16 PfxLog = 0;
  /// The cached Prefix.
  //char URITag[6] {};
};

#define DECL_MAPPING_X(TO, FROM)                                              \
  ALWAYS_INLINE static TO* X(FROM* Ptr) {                                     \
    return reinterpret_cast<TO*>(Ptr);                                        \
  }                                                                           \
  ALWAYS_INLINE static TO& X(FROM& Ref) {                                     \
    return *reinterpret_cast<TO*>(&Ref);                                      \
  }
#define DECL_UNMAPPING(TYPE)                                                  \
  ALWAYS_INLINE static TYPE* Unmap(TYPE::ValueType* Ptr) {                    \
    return stringmap_detail::mapValueToEntry(Ptr);                            \
  }                                                                           \
  ALWAYS_INLINE static const TYPE* Unmap(const TYPE::ValueType* Ptr) {        \
    return stringmap_detail::mapValueToEntry(Ptr);                            \
  }

#define DECL_MAPPINGS_X(REAL, FAKE)                                           \
  DECL_MAPPING_X(REAL, FAKE)                                                  \
  DECL_MAPPING_X(FAKE, REAL)                                                  \
  DECL_MAPPING_X(const REAL, const FAKE)                                      \
  DECL_MAPPING_X(const FAKE, const REAL)
#define DECL_MAPPINGS(REAL, FAKE)                                             \
  DECL_MAPPINGS_X(REAL, FAKE)                                                 \
  DECL_UNMAPPING(REAL)

/// Get a reference/pointer to the "Value Of" a map entry.
#define DECL_VOF(CV, QUAL, ...)                                               \
  template <typename T> ALWAYS_INLINE static                                  \
  CV T QUAL VOf(CV StringMapEntry<T> QUAL Entry) { return __VA_ARGS__; }

/// The string table used for encoding. Assumes all inputs it recieves are valid.
/// TODO: Check if we can get a more optimal memory layout.
class StringTable {
  /// The allocator shared internally.
  mutable exi::BumpPtrAllocator Alloc;
  /// Used to unique strings for lookup.
  // TODO: Figure out if necessary?
  exi::UniqueStringSaver NameCache;

  /// Used to cache!!
  /// TODO: Use to pack memory (and profile...)
  static constexpr usize kURIMax = 0xFFFFFF;
  /// Contains URI's ID and reverse mappings for prefixes.
  struct URIInfo {
    u32 URI = 0;
    /// The number of associated LocalNames.
    u32 LocalNames = 0;
    /// Iterate while recording the index to find the PfxID.
    SmallVec<PrefixInfo*, 2> PfxMap;

  public:
    /// Finds the index of an existing prefix mapping, otherwise appends.
    unsigned try_emplace(PrefixInfo* Pfx) {
      exi_assert(Pfx != nullptr);
      for (auto [Ix, Val] : exi::enumerate(PfxMap)) {
        if (Pfx == Val)
          return Ix;
      }

      PfxMap.emplace_back(Pfx);
      return PfxMap.size() - 1u;
    }
    bool contains(PrefixInfo* Pfx) const {
      for (auto* I : PfxMap) {
        if (I == Pfx)
          return true;
      }
      return false;
    }
    unsigned numMappedPrefixes() const {
      return PfxMap.size();
    }
  };
  /// Maps a URI to its associated ID.
  using URIMapType = BumpStringMap<URIInfo>;
  /// Stores the mapping between a URI and its associated ID.
  using URIEntry = URIMapType::value_type;

public:
  /// Maps a Prefix to its corresponding URI(s).
  using PrefixMapType = BumpStringMap<PrefixInfo>;
  /// Stores the mapping between a Prefix and its corresponding URI(s).
  using PrefixEntry = PrefixMapType::value_type;

private:
  /// Used to map URIs to IDs.
  URIMapType URIMap;
  /// Used to map Prefixes to URIs (and their IDs).
  PrefixMapType PrefixMap;

  // TODO: Add Deque<ExternAllocBumpStringMap<QualName*>>?
  
  static constexpr unsigned kURIStackElts = 8;
  /// Represents nested namespace contexts.
  using URIStack = SmallVec<PrefixInfo, 1>;
  /// Maps a PrefixEntry to a stack of URI values.
  using URIStackMapType = SmallDenseMap<const PrefixEntry*, URIStack, 8>;
  /// Wraps the `URIStackMapType`.
  struct URIStackMapHandler {
    Box<URIStackMapType> TheStacks = nullptr;
    const void* StackHandle = nullptr;
    // TODO: Profile cache...
#if EXI_ENCODE_URISTACK_CACHE
    SmallLRUCache<const PrefixEntry*, URIStack*, 2> TheCache;
#else
    std::pair<const PrefixEntry*, URIStack*> TheCache;
#endif
  private:
    EXI_COLD EXI_PRESERVE_MOST void initStackMap() {
      exi_relassert(empty(), "Cache already initialized.");
      this->TheStacks = std::make_unique<URIStackMapType>(1);
      this->loadStackHandle();
    }
    void loadStackHandle() {
      exi_invariant(!empty(), "Cache has not been initialized.");
      this->StackHandle = TheStacks->getPointerIntoBucketsArray();
    }

    void updateCacheEntries() {
      exi_invariant(isCacheOutOfDate(), "Cache already up to date.");
#if EXI_ENCODE_URISTACK_CACHE
      TheCache.update([this] (const PrefixEntry* K, URIStack*& V) {
        auto* Bucket = &*TheStacks->find(K);
        V = &Bucket->second;
      });
#else
      if (!isCacheEmpty()) {
        auto* Bucket = &*TheStacks->find(TheCache.first);
        TheCache.second = &Bucket->second;
      }
#endif
      this->loadStackHandle();
    }
    bool isCacheOutOfDate() const {
      exi_invariant(!empty(), "Cache has not been initialized.");
      return TheStacks->isPointerIntoBucketsArray(StackHandle);
    }
    ALWAYS_INLINE bool isCacheEmpty() const {
#if EXI_ENCODE_URISTACK_CACHE
      return TheCache.empty();
#else
      return TheCache.first == nullptr;
#endif   
    }

  public:
    /// Causes the stack to be setup if uninitialized, then returns the pointer.
    EXI_INLINE URIStackMapType* get() {
      if EXI_UNLIKELY(empty())
        this->initStackMap();
      return TheStacks.get();
    }
    /// Returns the stack pointer without initializing.
    inline Naked<URIStackMapType> getUnchecked() const {
      return TheStacks.get();
    }
    /// Causes the stack to be setup if uninitialized.
    inline URIStackMapType& operator*() {
      return *this->get();
    }
    /// Causes the stack to be setup if uninitialized.
    inline URIStackMapType* operator->() {
      return this->get();
    }
    inline URIStack& operator[](PrefixEntry* Pfx) {
      return operator*()[Pfx];
    }

    /// Adds an item to the cache.
    bool cache(const PrefixEntry* Key, URIStack* Value) {
      // FIXME: Only allow nullptrs in permissive mode?
      if EXI_NEVER(!Key || this->empty()) {
        // FIXME: Add warning log.
        return false;
      }
      URIStackMapType::AssertValidKey(Key);
      exi_relassert(TheStacks->isPointerIntoBucketsArray(Value));
      if (TheCache.set(Key, Value))
        return true;
      exi_unreachable("invalid cache entry.");
    }
    /// Explicitly removes an item from the cache.
    /// Returns whether or not it was found.
    bool uncache(const PrefixEntry* Key) {
#if EXI_ENCODE_URISTACK_CACHE
      if (TheCache.empty())
        return false;
      return TheCache.remove(Key);
#else
      if (TheCache.first != Key)
        return false;
      TheCache = {URIStackMapType::}
#endif
    }
    /// Checks if the cache is invalid, if it is, invalidate the entries.
    void updateCacheIfOutOfDate() {
      if EXI_UNLIKELY(StackHandle == nullptr) {
        exi_assert(!empty(), "URI stack has been initialized without "
                             "assigning the stack handle!");
        // Stack has not been initialized, exit.
        return;
      }
      if (isCacheOutOfDate())
        this->updateCacheEntries();
    }

    /// Removes empty stacks from the set to avoid allocations. `SmallDenseMap`
    /// allocates enough memory for 64 elements, which is unnecessary. You should
    /// really never have enough active nested contexts to trigger allocations.
    void cleanupUnusedStacks();

    EXI_INLINE bool empty() const {
      return TheStacks == nullptr;
    }
  };
  /// Maps a PrefixEntry to a stack of URIs representing nested namespace contexts.
  /// This is managed externally, as the string table has no knowledge of the format.
  /// Lazily initialized as many files will not require it.
  URIStackMapHandler URIStackMap = {};

  /// Represents LocalValues.
  using LocalValuesType = SmallDenseMap<STValueEntry*, CompactID, 8>;
  /// Represents the `[LNID, [LV...]]` tuple. LocalValues are lazily initialized
  /// unless told otherwise.
  struct LocalNameInfo {
    u32 LNID = 0;
    DtorOnlyBox<LocalValuesType> LVs;

    static LocalValuesType* GetLVsOrNull(StringTable* Tbl) {
      if (!Tbl)
        return nullptr;
      return new (Tbl->Alloc) LocalValuesType;
    }

    EXI_COLD void init(BumpPtrAllocator& BP) {
      exi_invariant(LVs == nullptr);
      LocalValuesType* Ptr = new (BP) LocalValuesType;
      LVs.reset(Ptr);
    }

  public:
    LocalNameInfo() = default;
    LocalNameInfo(URIEntry* ID, StringTable* Tbl = nullptr) :
     LNID(ID ? VOf(ID)->LocalNames++ : 0), LVs(GetLVsOrNull(Tbl)) {
    }

    u32 id() const { return LNID; }
    bool didInit() const { return LVs.get(); }

    LocalValuesType& get(const StringTable& Tbl) {
      return get(Tbl.Alloc);
    }

    LocalValuesType& get(BumpPtrAllocator& BP) {
      if EXI_UNLIKELY(!LVs)
        this->init(BP);
      return *LVs;
    }

    Option<LocalValuesType&> get() const {
      if EXI_UNLIKELY(!LVs)
        return std::nullopt;
      return *LVs;
    }
  };
  /// Maps a QName to LocalName data: `"URI$ln" -> [LNID, [LV...]]`.
  DenseMap<const QualName*, LocalNameInfo> LVMap;
  
  /// The value stored for each entry in the Value map.
  struct ValueInfo {
    /// The value's GlobalID.
    CompactID GID = 0;
    /// The latest LocalName using this Value.
    const QualName* Name = nullptr;
  };
  /// Maps a Value to its corresponding data.
  using ValueMapType = BumpStringMap<ValueInfo, /*IsOwned=*/true>;
  /// Stores the mapping between a Value and its corresponding data.
  using ValueEntry = ValueMapType::value_type;
  /// Handles the mapping from the string representation of a value to the
  /// value's data. Lookups aren't done through NameCache to reduce the number
  /// of searches required for that.
  EmbeddedClassCounter<ValueMapType> GValueMap;

  bool DidSetup : 1 = false;
  /// If the tables should wrap once reaching their capacity.
  bool WrappingValues : 1 = false;
  /// If URIs should be normalized.
  bool NormalizeURIs : 1 = false;

  ////////////////////////////////////////////////////////////////////////
  // Handle Mapping

  DECL_MAPPINGS(PrefixEntry,  STPrefixEntry)
  DECL_MAPPINGS(URIEntry,     STURIEntry)
  DECL_MAPPINGS(ValueEntry,   STValueEntry)
  DECL_MAPPINGS_X(InlineStr, QualName)

  DECL_VOF(, &, Entry.second)
  DECL_VOF(, *, &Entry->second)
  DECL_VOF(const, &, Entry.second)
  DECL_VOF(const, *, &Entry->second)

  /// Equivalent to invokes `(VOf ∘ X)(Val)`.
  ALWAYS_INLINE static decltype(auto) VOfX(auto&& Val) {
    return VOf(X(EXI_FWD(Val)));
  }
  /// Equivalent to invokes `(X ∘ Unmap)(Val)`.
  template <typename T>
  requires (!is_opaque_handle<std::remove_const_t<T>>)
  ALWAYS_INLINE static auto XUnmap(T* Val) {
    return X(Unmap(Val));
  }

public:
  StringTable();
  StringTable(const ExiOptions& Opts) : StringTable() {
    this->setup(Opts);
  }

  /// Sets up the initial encoder state.
  /// The signature will have to change when schemas are introduced.
  void setup(const ExiOptions& Opts);

  static CachedHashStrRef prehash(StrRef S) {
    const u32 Hash = StringMapImpl::hash(S);
    return CachedHashStrRef(S, Hash);
  }

  EXI_INLINE static StrRef GetURI(const STURIEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return X(Entry)->first();
  }
  EXI_INLINE static u32 GetID(const STURIEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return X(Entry)->second.URI;
  }

private:
  // TODO: Finish design...

  ////////////////////////////////////////////////////////////////////////
  // Prefixes

  static PrefixInfo MakePrefix(PrefixInfo* Pfx, URIEntry* URI) {
    exi_invariant(Pfx->Link != X(URI),
                 "Prefix has already been mapped to this URI.");
    const u16 PfxID = IntCast<u16>(VOf(URI)->try_emplace(Pfx));
    return PrefixInfo {
      .Link = X(URI),
      .WithURI = VOf(URI)->URI,
      .Pfx = PfxID,
      .PfxLog = CompactIDLog2(PfxID)
    };
  }

  /// Pushes the URI context currently associated with the given prefix to the
  /// stack, then replaces it with the given URI.
  void pushURIContext(PrefixEntry* EPfx, URIEntry* URI);

  /// Pops the last URI context from the prefix's associated stack, then
  /// restores to that state.
  void popURIContext(PrefixEntry* EPfx);

  /// A simple heuristic for determining if the map is likely to reallocate.
  /// Only considers the case when 3/4 full.
  static bool GuessIfMapIsReallocating(const URIStackMapType& Map) {
    const unsigned Buckets = Map.approximate_capacity();
    if (Buckets > kURIStackElts)
      // Already reallocated, who cares...
      return false;
    exi_invariant(Buckets == kURIStackElts);
    const unsigned Elts = Map.size() + 1u;
    return EXI_UNLIKELY(Elts * 4 >= kURIStackElts * 3u);
  }

  /// Removes empty stacks from the set to avoid allocations when capacity will
  /// be reached on the next insertion.
  /// TODO: Profile, may not be necessary.
  void cleanupURIStacks();

  ////////////////////////////////////////////////////////////////////////
  // Qualified Names

  enum URITagInfo : usize {
    kUTagBase = 32,
    kUTagBits = Log2_64(kUTagBase),
    kUTagMask = kUTagBase - 1,
    kUTagIters = (kUTagBase / kUTagBits) + 1,
  };

  /// Checks if `C` is in the range of our base-32 character mappings.
  ALWAYS_INLINE static constexpr bool IsURITagChar(char C) {
    return (C >= '0') && (C <= 'O');
  }

  template <bool CheckID = true>
  bool isValidQualifiedName(StrRef S) const {
    if EXI_UNLIKELY(S.size() < 3)
      return false;
    
    const char* I = S.begin();
    auto* const E = S.end();

    [[maybe_unused]] u32 ID {};
    while (IsURITagChar(*I)) {
      if constexpr (CheckID)
        ID = (ID << kUTagBits) | u32(*I - '0');
      if EXI_UNLIKELY(++I == E - 1)
        return false;
    }

    if constexpr (CheckID) {
      if EXI_UNLIKELY(ID >= URIMap.size())
        return false;
    }
    return (*I == '$') && (I + 1 != E);
  }

  static void WriteURITag(u32 ID, SmallVecImpl<char>& Buf) {
    if EXI_LIKELY(ID < 32) {
      Buf.push_back('0' + ID);
      return;
    }

    for (int Ix = 0; Ix < int(kUTagIters); ++Ix) {
      Buf.push_back('0' + (ID & kUTagMask));
      ID >>= kUTagBits;
      if EXI_LIKELY(ID == 0)
        return;
    }
  }

  EXI_INLINE void writeURITagChecked(u32 ID, SmallVecImpl<char>& Buf) const {
    exi_invariant(ID < URIMap.size());
    return WriteURITag(ID, Buf);
  }

  ////////////////////////////////////////////////////////////////////////
  // Uniquing

  template <bool CheckID = true>
  EXI_INLINE const QualName* internQualName(StrRef Raw) {
    exi_invariant(isValidQualifiedName<CheckID>(Raw));
    return X(NameCache.saveRaw(Raw));
  }

  const QualName* internQualName(u32 URI, StrRef LocalName);

  /// Gets a new (URI*, DidInsert) pair from a URI.
  std::pair<URIEntry*, bool> createURIOnly(CachedHashStrRef URI) {
    auto [It, DidInsert] = URIMap.try_emplace(URI);
    if (DidInsert) {
      // Since the item was already inserted, decrement.
      It->second.URI = URIMap.size() - 1;
      // FIXME: Update log size?
    }
    return {&*It, DidInsert};
  }
  /// Gets a new (URI*, DidInsert) pair from a URI.
  std::pair<URIEntry*, bool> createURIOnly(StrRef URI) {
    /// Hash needs to be computed anyways, skip a step...
    return createURIOnly(prehash(URI));
  }

  /// Gets a new (URI*, Pfx*?) pair from a URI and Prefix.
  std::pair<URIEntry*, PrefixEntry*>
   createURI(CachedHashStrRef URI, Option<StrRef> Pfx = std::nullopt);
  /// Gets a new (URI*, Pfx*?) pair from a URI and Prefix.
  std::pair<URIEntry*, PrefixEntry*>
   createURI(StrRef URI, Option<StrRef> Pfx = std::nullopt) {
    return createURI(prehash(URI), Pfx);
  }
  
  // Add other stuff...

  ////////////////////////////////////////////////////////////////////////
  // Batch Initialization

public:
  /// Only really used for initialization. Maps `"pfx:[ln]" -> "URI$pfx:ln"`.
  struct NameMapping {
    StrRef LocalName;
    StrRef QualifiedName;
  };

private:
  /// Creates the initial entries for the string table. The values inserted
  /// depend on the schema.
  void createInitialEntries(bool UsesSchema);
  /// Appends LocalNames to the provided URI.
  void appendLocalNames(URIEntry* ID, ArrayRef<NameMapping> LNMappings);
  /// Appends LocalNames to the provided URI.
  [[deprecated("Use the ID bound variant")]]
  void appendLocalNames(ArrayRef<NameMapping> LNMappings);
  /// Initializes a unique LocalName entry.
  void initLocalName(URIEntry* URI, const QualName* ID);
};

#undef DECL_MAPPING_X
#undef DECL_UNMAPPING
#undef DECL_MAPPINGS_X
#undef DECL_MAPPINGS
#undef DECL_VOF

} // namespace encode

} // namespace exi
