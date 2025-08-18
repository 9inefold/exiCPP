//===- exi/Encode/StringTable.hpp ------------------------------------===//
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
/// This file implements the encoder's StringTable.
/// String tables have no understanding of the EXI format (other than length),
/// they simply cache the provided values.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Box.hpp>
#include <core/Common/CachedHashString.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/FoldingSet.hpp>
#include <core/Common/Naked.hpp>
#include <core/Common/Result.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/MathExtras.hpp>
#include <core/Support/StringSaver.hpp>
#include <exi/Basic/CompactID.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Encode/D/StringTableMappings.mac>
#include <exi/Encode/BodyEncoderAlloc.hpp>
#include <exi/Encode/StringTableHandles.hpp>
#include <type_traits>

// TODO: Refactor to use embedded counters with RTTI handles
// TODO: Replace some assertions with Throw(...)?

/// If the cache should be LRU (a single pair otherwise).
#define EXI_ENCODE_URISTACK_CACHE 0

namespace exi {

struct ExiOptions;
class NSContextStack;

/// Defines utilities for encoding EXI.
namespace encode {
class StringTable;

using TableBumpAllocator = exi::EncoderBumpAllocator;

template <typename Value, bool IsOwned = false>
using BumpStringMap = StringMap<Value,
  std::conditional_t<IsOwned, TableBumpAllocator, TableBumpAllocator&>>;

/// Allows `CachedHashStrRef`s to be implicitly constructed. Since we use them
/// internally, it makes passing arguments simpler.
class ImplicitHashStrRef : public CachedHashStrRef {
public:
  using CachedHashStrRef::CachedHashStrRef;
  ImplicitHashStrRef(StrRef S) : CachedHashStrRef(S, StringMapImpl::hash(S)) {}
  ImplicitHashStrRef(CachedHashStrRef S) : CachedHashStrRef(S) {
    exi_invariant(S.hash() == StringMapImpl::hash(S.val()));
  }
  constexpr bool operator==(const CachedHashStrRef& S) const {
    return CachedHashStrRef::equals(S);
  }
};

/// Using `u32`, because if you have 4 billion uris... wtf.
// TODO: Handle cases where a prefix mapping is rescinded.
struct PrefixInfo {
  STURIEntry* Link = nullptr;
  /// The ID of the URI.
  u32 WithURI = max_v<u32>;
  /// The ID of the prefix.
  u16 Pfx = max_v<u16>;
  /// The cached qname prefix log.
  u16 PfxLogQ = 0;
public:
  /// Checked if the Prefix is synced with `Link`.
  inline bool isSyncedWithURI() const;
  /// Syncs `PfxLogQ` with `Link->PfxMap`s size.
  inline void syncWithURI();
  /// Returns URI if valid.
  u32 uri() const {
    exi_expensive_invariant(this->isSyncedWithURI());
    if EXI_NEVER(WithURI == max_v<u32>)
      Throw<uninit_error>("URI is uninitialized!");
    return this->WithURI;
  }
  /// Returns Prefix if valid.
  u16 pfx() const {
    exi_expensive_invariant(this->isSyncedWithURI());
    if EXI_NEVER(Pfx == max_v<u16>)
      Throw<uninit_error>("Prefix is uninitialized!");
    return this->Pfx;
  }
};

/// Represents the `[LNID, [LV...]]` tuple.
class LocalNameInfo : public FoldingSetNode {
  friend class StringTable;
  using LVType = SmallDenseMap<STValueEntry*, CompactID, 8>;
  /// The URI associated with this LocalName.
  STURIEntry* URI;
  /// The ID of the associated URI.
  unsigned WithURI = 0;
  /// The id of this LocalName.
  unsigned LNID = 0;
  /// The string value of the LocalName.
  const InlineStr* LocalName;
  /// The LocalValues associated with this LocalName.
  mutable LVType LVs;

  /// Gets `WithURI` for `ID`.
  inline static unsigned CheckURI(STURIEntry* ID);
  /// Gets `LNID` for `ID`, then increments.
  inline static unsigned GetLNID(STURIEntry* ID);

public:
  inline LocalNameInfo(STURIEntry* ID, const InlineStr* LocalName);

  unsigned uri() const { return WithURI; }
  unsigned id() const { return LNID; }
  STURIEntry* link() const { return URI; }

  StrRef name() const { return LocalName->str(); }
  inline StrRef uriName() const;

  LVType& values() const { return LVs; }
  unsigned size() const { return LVs.size(); }
  unsigned log() const { return ID_Log2(LVs.size()); }

  std::pair<CompactID, bool> addEntry(STValueEntry* GV) {
    exi_invariant(GV != nullptr);
    const CompactID LVID = LVs.size();
    auto [It, DidInsert] = LVs.try_emplace(GV, LVID);
    if EXI_UNLIKELY(!DidInsert)
      return {It->second, false};
    return {LVID, true};
  }

  void Profile(FoldingSetNodeID& ID) const {
    ID.AddString(LocalName->str());
    ID.AddInteger(WithURI);
  }
};

/// The string table used for encoding. Assumes all inputs it recieves are valid.
/// TODO: Check if we can get a more optimal memory layout.
class StringTable {
  friend class exi::NSContextStack;
  friend class LocalNameInfo;
  /// The allocator shared internally.
  mutable TableBumpAllocator Alloc;
  /// Used to unique strings for lookup.
  // TODO: Figure out if necessary?
  exi::UniqueStringSaver NameCache;

  /// These function are before everything else, as they cause an ICE on Clang.
  /// For "good" reason, they're used before auto can be deduced.

  /// Equivalent to invoking `(VOf ∘ X)(Val)`.
  ALWAYS_INLINE static decltype(auto) VOfX(auto&& Val) {
    return VOf(X(EXI_FWD(Val)));
  }
  /// Equivalent to invoking `(X ∘ Unmap)(Val)`.
  template <typename T>
  requires (!is_opaque_handle<std::remove_const_t<T>>)
  ALWAYS_INLINE static auto XUnmap(T* Val) {
    return X(Unmap(Val));
  }

public:
  /// TODO: Use to pack memory? (and profile...)
  static constexpr u32 kURIMax = 0xFFFFFF;
  static constexpr u32 kURIUninit = max_v<u32>;
  /// Contains URI's ID and reverse mappings for prefixes.
  /// TODO: Handle Prefix unbinding!
  struct URIInfo {
    u32 URI = kURIUninit;
    /// The number of associated LocalNames.
    u32 LocalNames = 0;
    /// Iterate while recording the index to find the PfxID.
    SmallVec<PrefixInfo*, 2> PfxMap;

    static u16 Log2Q(unsigned ID) {
      return ID_AddOffsetLog2<0>(ID);
    }
    static bool NeedsBroadcast(unsigned NewID) {
      // Changes up the normal is_pow2 trick, as the newest ID is the old size
      // of the prefix array. Since size is always `NewestID + 1`, I tweaked it
      // to handle that.
      return ((NewID + 1u) & NewID) == 0;
    }

  private:
    ALWAYS_INLINE void bindPfx(PrefixInfo* Pfx, u16 ID, unsigned Size) const {
      Pfx->Link     = XUnmap(const_cast<URIInfo*>(this));
      Pfx->Pfx      = ID;
      Pfx->PfxLogQ  = Log2Q(Size);
      Pfx->WithURI  = uri();
    }
    template <bool BindToThis>
    inline void bindNewPfx(PrefixInfo* Pfx, u16 ID) const {
      if constexpr (BindToThis) {
        exi_invariant(Pfx == PfxMap.back());
        this->bindPfx(Pfx, ID, static_cast<unsigned>(ID));
      }
    }
    template <bool BindToThis>
    inline void bindOldPfx(PrefixInfo* Pfx, u16 ID) const {
      if constexpr (BindToThis)
        this->bindPfx(Pfx, ID, PfxMap.size());
    }

    EXI_COLD void broadcastLog(u16 Log) {
      // BUG: This ICEs on Clang...
      STURIEntry* const self = XUnmap(this);
      for (PrefixInfo* PI : PfxMap) {
        // Skips over any prefixes not currently mapped to this URI.
        if EXI_LIKELY(PI->Link == self)
          PI->PfxLogQ = Log;
      }
    }
    // FIXME: Consider what happens when inserting empty.
    template <bool BindToThis>
    EXI_COLD u16 emplaceAndBroadcast(PrefixInfo* Pfx) {
      exi_expensive_invariant(!this->contains(Pfx));
      const u16 ID = PfxMap.size();
      if EXI_NEVER(ID >= max_v<u16>)
        Throw<range_error>("Exceeded the maximum number of PfxIDs!");

      PfxMap.emplace_back(Pfx);
      bindNewPfx<BindToThis>(Pfx, ID);
      // Only happens once, quite unlikely.
      if EXI_UNLIKELY(ID == 0)
        return 0;
      // Check if update broadcast is required.
      // This gets less likely the more prefixes are added.
      if EXI_UNLIKELY(NeedsBroadcast(ID))
        this->broadcastLog(Log2Q(ID));
      return ID;
    }
    template <bool BindToThis = false>
    u16 emplaceImpl(PrefixInfo* Pfx) {
      exi_assert(Pfx != nullptr);
      for (auto [ID, Val] : exi::enumerate(PfxMap)) {
        if (Pfx != Val)
          continue;
        bindOldPfx<BindToThis>(Pfx, ID);
        return ID;
      }
      return emplaceAndBroadcast<BindToThis>(Pfx);
    }

  public:
    /// Finds the index of an existing prefix mapping, otherwise appends.
    u16 try_emplace(PrefixInfo* Pfx) {
      tail_return this->emplaceImpl<false>(Pfx);
    }
    /// Finds the index of an existing prefix mapping and rebinds.
    /// Otherwise, appends and creates a new binding.
    u16 try_emplace_and_bind(PrefixInfo* Pfx) {
      tail_return this->emplaceImpl<true>(Pfx);
    }

    EXI_COLD void recalculateLog() {
      this->broadcastLog(Log2Q(PfxMap.size()));
    }

    bool contains(const PrefixInfo* Pfx) const {
      for (auto* I : PfxMap) {
        if (I == Pfx)
          return true;
      }
      return false;
    }
    ALWAYS_INLINE bool contains(const STPrefixEntry* Pfx) const {
      return contains(VOfX(Pfx));
    }
    unsigned numMappedPrefixes() const {
      return PfxMap.size();
    }
    u16 pfxLogQ() const {
      return Log2Q(numMappedPrefixes());
    }
    u16 pfxLog() const {
      return ID_AddOffsetLog2<1>(numMappedPrefixes());
    }
    u32 uri() const {
      if EXI_NEVER(URI == kURIUninit)
        Throw<uninit_error>("URI is uninitialized!");
      return this->URI;
    }
  };
  /// Maps a URI to its associated ID.
  using URIMapType = BumpStringMap<URIInfo>;
  /// Stores the mapping between a URI and its associated ID.
  using URIEntry = URIMapType::value_type;

  friend struct PrefixInfo;
  /// Maps a Prefix to its corresponding URI(s).
  using PrefixMapType = BumpStringMap<PrefixInfo>;
  /// Stores the mapping between a Prefix and its corresponding URI(s).
  using PrefixEntry = PrefixMapType::value_type;

private:
  /// Used to map URIs to IDs.
  IntrusiveLogCounter<URIMapType, 1> URIMap;
  /// Used to map Prefixes to URIs (and their IDs).
  PrefixMapType PrefixMap;

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
    std::pair<const PrefixEntry*, URIStack*> TheCache;
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
      if EXI_LIKELY(!isCacheEmpty()) {
        auto* Bucket = &*TheStacks->find(TheCache.first);
        TheCache.second = &Bucket->second;
      }
      this->loadStackHandle();
    }
    bool isCacheOutOfDate() const {
      exi_invariant(!empty(), "Cache has not been initialized.");
      return TheStacks->isPointerIntoBucketsArray(StackHandle);
    }
    ALWAYS_INLINE bool isCacheEmpty() const {
      return TheCache.first == nullptr;
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
      return lookup(Pfx);
    }

    bool contains(PrefixEntry* Pfx) const {
      exi_invariant(!empty());
      if (TheCache.first == Pfx)
        return true;
      return TheStacks->contains(Pfx);
    }
    URIStack& lookup(PrefixEntry* Pfx) {
      auto& Stacks = *get();
      if (TheCache.first == Pfx)
        return *TheCache.second;
      auto& Out = Stacks[Pfx];
      cache(Pfx, &Out);
      return Out;
    }

    /// Adds an item to the cache.
    bool cache(const PrefixEntry* Key, URIStack* Value) {
      //if EXI_NEVER(empty())
      //  return false;
      // FIXME: Only allow nullptrs in permissive mode?
      exi_invariant(Key && Value);
      URIStackMapType::AssertValidKey(Key);
      exi_relassert(TheStacks->isPointerIntoBucketsArray(Value));
      TheCache = {Key, Value};
      return true;
    }
    /// Explicitly removes an item from the cache.
    /// Returns whether or not it was found.
    bool uncache(const PrefixEntry* Key) {
      exi_invariant(Key != nullptr);
      if (TheCache.first != Key)
        return false;
      TheCache = {nullptr, nullptr};
      return true;
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

public:
  /// Represents LocalValues.
  using LocalValuesType = LocalNameInfo::LVType;
  /// Maps a `(URI, "ln")` pair to its LocalValues.
  using LNMapType = FoldingSet<LocalNameInfo>;
  /// Stores the LocalName.
  using LNEntry = LocalNameInfo;

  /// The value stored for each entry in the Value map.
  struct ValueInfo {
    /// The value's GlobalID.
    CompactID GID = 0;
    /// The latest `[LocalValues, ID]` associated with this Value.
    std::pair<LocalNameInfo*, u32> RecentLV = {nullptr, 0xFFFFFFFF};
  };
  /// Maps a Value to its corresponding data.
  using ValueMapType = BumpStringMap<ValueInfo, /*IsOwned=*/true>;
  /// Stores the mapping between a Value and its corresponding data.
  using ValueEntry = ValueMapType::value_type;

private:
  /// Maps a QName to LocalName data: `URI:"ln" -> [LNID, [LV...]]`.
  LNMapType LVMap;
  /// Handles the mapping from the string representation of a value to the
  /// value's data. Lookups aren't done through NameCache to reduce the number
  /// of searches required for that.
  IntrusiveLogCounter<ValueMapType> GValueMap;

  PrefixEntry* AT_NIL  = nullptr; // Used for unprefixed attributes.
  URIEntry* AT_NIL_URI = nullptr; // Used for unprefixed attributes.

  PrefixEntry* Pfx_NIL = nullptr; // xmlns="..."
  PrefixEntry* Pfx_xml = nullptr; // xmlns:xml="..."
  PrefixEntry* Pfx_xsi = nullptr; // xmlns:xsi="..."
  PrefixEntry* Pfx_xsd = nullptr; // xmlns:xsd="..."

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
  DECL_MAPPINGS_X(InlineStr,  QualName)

  DECL_VOF(, &, Entry.second)
  DECL_VOF(, *, &Entry->second)
  DECL_VOF(const, &, Entry.second)
  DECL_VOF(const, *, &Entry->second)

  // See VOfX(auto&&) further up...
  // See XUnmap(auto*) further up...

  /// Gets the type mapping `X(T) -> XType`.
  template <typename T>
  using XType = std::remove_reference_t<
    decltype(StringTable::X(std::declval<T&>()))>;
  
  /// Type used for new entries into the table.
  template <typename T> class XEntry {
    using type = std::remove_const_t<T>;
    using x_type = XType<T>;
    static_assert(!is_opaque_handle<type>,
                  "Real type must be used!");
    T* Data = nullptr;
    bool Inserted = false;

  public:
    explicit XEntry(T* Data, bool In = false) : Data(Data), Inserted(In) {}
    explicit XEntry(x_type* Data, bool In = false) : XEntry(X(Data), In) {}

    template <typename U> requires std::convertible_to<U*, T*>
    XEntry(std::pair<U*, bool> V) : XEntry(V.first, V.second) {}

    template <typename U> requires std::convertible_to<U*, x_type*>
    XEntry(std::pair<U*, bool> V) : XEntry(X(V.first), V.second) {}

    T* data() const { return Data; }
    x_type* xdata() const { return X(Data); }
    bool inserted() const { return Inserted; }
  };

public:
  StringTable();
  StringTable(const ExiOptions& Opts) : StringTable() { this->setup(Opts); }
  ~StringTable();

  /// Sets up the initial encoder state.
  /// The signature will have to change when schemas are introduced.
  void setup(const ExiOptions& Opts);

  ALWAYS_INLINE static CachedHashStrRef Hash(StrRef S) {
    const u32 Hash = StringMapImpl::hash(S);
    return CachedHashStrRef(S, Hash);
  }

  static CachedHashStrRef prehash(StrRef S) {
    // TODO: Profile
    if (S.empty())
      return GetEmptyHashString();
    return Hash(S);
  }
  ALWAYS_INLINE static CachedHashStrRef
   prehash(CachedHashStrRef S) { return S; }

  static CachedHashStrRef GetEmptyHashString() {
    static CachedHashStrRef S = Hash(""_str);
    return S;
  }
  static CachedHashStrRef GetXMLNSHashString() {
    static CachedHashStrRef S = Hash("xmlns"_str);
    return S;
  }

  EXI_INLINE static STURIEntry* GetURIEntry(const STPrefixEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->Link;
  }
  EXI_INLINE static StrRef GetPfx(const STPrefixEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return X(Entry)->first();
  }
  EXI_INLINE static unsigned GetID(const STPrefixEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->pfx();
  }

  EXI_INLINE static StrRef GetURI(const STURIEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return X(Entry)->first();
  }
  EXI_INLINE static unsigned GetID(const STURIEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->URI;
  }
  EXI_INLINE static usize GetPfxCount(const STURIEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->PfxMap.size();
  }
  static STPrefixEntry* GetAnyPfx(const STURIEntry* Entry) {
    if (!Entry)
      return nullptr;
    auto& PMap = VOfX(Entry)->PfxMap;
    if (PMap.empty())
      return nullptr;
    exi_invariant(PMap.back(), "Invalid Prefix entry!");
    return XUnmap(PMap.back());
  }

  EXI_INLINE static URIEntry* GetLink(const LNEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return X(Entry->link());
  }

  EXI_INLINE static CompactID GetGlobalID(const STValueEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->GID;
  }
  EXI_INLINE static unsigned GetLocalID(const STValueEntry* Entry) {
    exi_invariant(Entry != nullptr);
    return VOfX(Entry)->RecentLV.second;
  }

  ////////////////////////////////////////////////////////////////////////
  // Setters

  /// The result of entring a Namespace context.
  /// TODO: Use PointerIntUnion + enums to pack better?
  class NSContext {
    friend class StringTable;
    /// The URI.
    URIEntry* URI = nullptr;
    /// The OPTIONAL Prefix value.
    PrefixEntry* Pfx = nullptr;
    /// If this is a newly inserted URI.
    bool NewURI : 1 = false;
    /// If this is a newly inserted Prefix.
    bool NewPfx : 1 = false;
    /// If this is an anonymous namespace.
    bool IsAnonymous : 1 = false;
    /// If this overwrites an existing context.
    bool IsOverwrite : 1 = false;

    Option<bool> isSelfAnonymous() const {
      if (Pfx == nullptr)
        return std::nullopt;
      return Pfx->getKey().empty();
    }
    bool isAnonymousForAssert(bool Val) const {
      if (auto Chk = isSelfAnonymous())
        return *Chk == Val;
      return true;
    }

  public:
    NSContext(URIEntry* URI, bool NewURI) :
     URI(URI), NewURI(NewURI) {
      exi_assert(URI != nullptr);
    }
    explicit NSContext(XEntry<URIEntry> Val) :
     NSContext(Val.data(), Val.inserted()) {
    }
    NSContext& Prefix(PrefixEntry* Pfx, bool NewPfx) {
      exi_assert(Pfx != nullptr);
      exi_invariant(this->unbound());
      this->Pfx = Pfx;
      this->NewPfx = NewPfx;
      return *this;
    }
    NSContext& Prefix(XEntry<PrefixEntry> Val) {
      return Prefix(Val.data(), Val.inserted());
    }
    NSContext& Unbound(bool Val = true) {
      exi_invariant(unbound() == Val);
      return *this;
    }
    NSContext& Anonymous(bool Val) {
      exi_invariant(isAnonymousForAssert(Val),
                   "Input value does not match prefix!");
      this->IsAnonymous = Val;
      return *this;
    }
    NSContext& Overwrite(bool Val = true) {
      this->IsOverwrite = Val;
      return *this;
    }

    /// Gets the URI.
    STURIEntry* uri() const { return X(URI); }
    /// Gets the OPTIONAL Prefix.
    STPrefixEntry* pfx() const { return X(Pfx); }
    /// Gets the URI's string value.
    StrRef uriName() const { return URI->getKey(); }
    /// Gets the Prefix's string value, if available.
    StrRef pfxName() const { return Pfx ? Pfx->getKey() : ""_str; }
    /// Is an unbound URI?
    bool unbound() const { return !Pfx; }
    /// Is a newly inserted URI?
    bool newUri() const { return NewURI; }
    /// Is a newly inserted Prefix?
    bool newPfx() const { return NewPfx; }
    /// Is an anonymous (unnamed) namespace?
    bool anonymous() const { return IsAnonymous; }
    /// Does the Prefix overwrite an existing URI mapping?
    bool overwrite() const { return IsOverwrite; }
  };

  /// For declaring namespaces in a schema.
  NSContext declareURI(StrRef URI) {
    return createURI(prehash(URI), std::nullopt);
  }
  NSContext declareURI(CachedHashStrRef URI) {
    return createURI(URI, std::nullopt);
  }

  /// Creates a new URI.
  STURIEntry* addURI(ImplicitHashStrRef URI) {
    auto [UI, IsNewURI] = createURIOnly(URI);
    // TODO: LOG_WARN("URI '{}' already existed.")
    return X(UI);
  }
  /// Creates a new Prefix entry for a URI.
  STPrefixEntry* addPrefix(STURIEntry* URI, ImplicitHashStrRef Pfx) {
    exi_invariant(URI != nullptr);
    auto [PI, IsNewPfx] = createPfxOnly(Pfx);
    if (IsNewPfx)
      BindPrefixToNewURI(&PI->second, X(URI));
    else
      pushURIContext(PI, X(URI));
    return X(PI);
  }
  /// Creates a new LocalName for a URI.
  LocalNameInfo* addLocalName(STURIEntry* URI, StrRef Name, LocalNameInsert*);
  /// Creates a new LocalValue for a GV.
  STValueEntry* addLocalValue(LocalNameInfo* LN, STValueEntry* GV);
  /// Creates a new GlobalValue and LocalValue for `Value`.
  STValueEntry* addValue(LocalNameInfo* LN, CachedHashStrRef Value);

  /// When encountering a `xmlns:[Pfx]=[URI]`.
  NSContext enterNamespace(ImplicitHashStrRef Pfx, ImplicitHashStrRef URI) {
    return createURI(URI, Pfx);
  }
  /// When adding a known mapping.
  void enterNamespace(STURIEntry* URI, STPrefixEntry* Pfx) {
    pushURIContext(X(Pfx), X(URI));
  }
  /// When exiting a scoped namespace context.
  void exitNamespace(STPrefixEntry* Pfx) {
    exi_invariant(Pfx != nullptr);
    return popURIContext(X(Pfx));
  }
  // TODO: Add method for normal users to access?

  ////////////////////////////////////////////////////////////////////////
  // Getters

  /// Returns the `PrefixEntry` for `Pfx`, if it exists.
  /// @tparam IsAttribute If the lookup is for an attribute.
  template <bool IsAttribute = false>
  STPrefixEntry* lookupPfx(ImplicitHashStrRef Pfx) {
    if (Pfx == GetEmptyHashString())
      return X(IsAttribute ? AT_NIL : Pfx_NIL);
    auto It = PrefixMap.find(Pfx);
    if (It == PrefixMap.end())
      return nullptr;
    return X(&*It);
  }
  /// Returns the `PrefixEntry` for `Pfx`, if it exists.
  /// @tparam IsAttribute If the lookup is for an attribute.
  template <bool IsAttribute = false>
  STPrefixEntry* lookupPfxForURI(STURIEntry* URI, ImplicitHashStrRef Pfx) {
    if (auto* PfxV = lookupPfx<IsAttribute>(Pfx)) {
      if (VOfX(PfxV)->Link == URI)
        return PfxV;
      else if (VOfX(URI)->contains(PfxV))
        return PfxV;
    }
    return nullptr;
  }
  /// Returns the `URIEntry` for `URI`, if it exists.
  STURIEntry* lookupURI(ImplicitHashStrRef URI) {
    auto It = URIMap->find(URI);
    if (It == URIMap->end())
      return nullptr;
    return X(&*It);
  }
  /// Gets `URIEntry*` for a given Prefix.
  /// @tparam IsAttribute If the lookup is for an attribute.
  template <bool IsAttribute = false>
  STURIEntry* getURIFromPfx(ImplicitHashStrRef Pfx) {
    if constexpr (IsAttribute)
      if (Pfx == GetEmptyHashString())
        return getUnprefixedATURI();
    auto It = PrefixMap.find(Pfx);
    exi_assert(It != PrefixMap.end());
    return It->second.Link;
  }

  /// Checks if a LocalName exists for a given `URI:"Name"`.
  /// If one doesn't, returns an insertion point.
  Result<LocalNameInfo*, LocalNameInsert*>
   lookupLocalName(STURIEntry* URI, StrRef Name);
  /// Returns the `ValueEntry`, if one exists. The bool marks if the value was
  /// global, or bound to the `LocalName` `LN`.
  std::pair<STValueEntry*, bool>
   lookupLocalValue(LocalNameInfo* LN, ImplicitHashStrRef Value);

  STPrefixEntry* getUnprefixedATPrefix() const { return X(AT_NIL); }
  STURIEntry* getUnprefixedATURI() const { return X(AT_NIL_URI); }

  ////////////////////////////////////////////////////////////////////////
  // Log Getters

  /// Returns the number of bits needed for Prefixes of the given URI.
  static unsigned getPfxLog(STURIEntry* URI) {
    return VOfX(URI)->pfxLog();
  }
  /// Returns the number of bits needed for the given Prefix.
  static unsigned getPfxLog(STPrefixEntry* Pfx) {
    if (auto* URI = VOfX(Pfx)->Link)
      return getPfxLog(URI);
    return 0;
  }
  /// Returns the number of bits needed for Prefix of the given URI.
  static unsigned getPfxLogQ(STURIEntry* URI) {
    exi_invariant(URI != nullptr, "Invalid URI!");
    return VOfX(URI)->pfxLogQ();
  }
  /// Returns the number of bits needed for the given Prefix.
  static unsigned getPfxLogQ(STPrefixEntry* Pfx) {
    auto* PfxV = VOfX(Pfx);
    if (auto* URI = PfxV->Link) {
      exi_invariant(PfxV->isSyncedWithURI());
      return PfxV->PfxLogQ;
    }
    return 0;
  }
  /// Returns an `(ID, Log)` pair for a Prefix.
  static std::pair<u32, u32> getPfxIDAndLog(STPrefixEntry* Pfx) {
    return {GetID(Pfx), getPfxLog(Pfx)};
  }
  /// Returns an `(ID, Log)` pair for a Prefix.
  static std::pair<u32, u32> getPfxIDAndLogQ(STPrefixEntry* Pfx) {
    return {GetID(Pfx), getPfxLogQ(Pfx)};
  }

  /// Returns the number of bits needed for URIs.
  unsigned getURILog() const {
    return URIMap.bits();
  }
  /// Returns an `(ID, Log)` pair for a URI.
  std::pair<u32, u32> getURIIDAndLog(STURIEntry* URI) const {
    return {GetID(URI), getURILog()};
  }

  /// Returns the number of bits needed for URIs.
  unsigned getLocalNameLog(STURIEntry* URI) const {
    return ID_Log2(VOfX(URI)->LocalNames);
  }

  /// Returns the number of bits needed for GVs.
  unsigned getGlobalValueLog() const {
    return GValueMap.bits();
  }

  ////////////////////////////////////////////////////////////////////////
  // Logging

private:
  ////////////////////////////////////////////////////////////////////////
  // Prefixes

  /// Checks if a prefix is valid.
  void CheckIsValidPrefix(CachedHashStrRef Pfx) const {
    if EXI_NEVER(GetXMLNSHashString().equals(Pfx))
      Throw<argument_error>("'xmlns' is not a valid prefix!");
    // TODO: Add other prefix validity checks?
  }

  /// Binds prefix to a uri it isn't already bound to.
  static u16 BindPrefixToNewURI(PrefixInfo* Pfx, URIEntry* URI) {
    exi_invariant(Pfx->Link != X(URI),
                 "Prefix has already been mapped to this URI.");
    return VOf(URI)->try_emplace_and_bind(Pfx);
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
  // Uniquing

  /// Gets a new `(URI*, IsNewURI)` from a URI.
  std::pair<URIEntry*, bool> createURIOnly(CachedHashStrRef URI);
  /// Gets a new `(URI*, IsNewURI)` from a URI.
  std::pair<URIEntry*, bool> createURIOnly(StrRef URI) {
    /// Hash needs to be computed anyways, skip a step...
    return createURIOnly(prehash(URI));
  }

  /// Gets a new (URI*, Pfx*?) pair from a URI and Prefix.
  NSContext createURI(CachedHashStrRef URI,
                      Option<CachedHashStrRef> Pfx = std::nullopt);
  /// Gets a new (URI*, Pfx*?) pair from a URI and Prefix.
  NSContext createURI(StrRef URI, Option<StrRef> Pfx = std::nullopt) {
    return createURI(prehash(URI), Pfx.transform([](StrRef S) {
      return StringTable::prehash(S);
    }));
  }
  /// Gets a new (URI*, Pfx*?) pair from a URI and Prefix.
  /// Used during initialization to avoid usage of potentially uninitialized data.
  /// Simpler than createURI, as it assumes all inputs are simple and valid.
  NSContext createURIForInit(StrRef URI, StrRef Pfx);

  std::pair<PrefixEntry*, bool> createPfxOnly(CachedHashStrRef Pfx) {
    CheckIsValidPrefix(Pfx);
    if (GetEmptyHashString().equals(Pfx))
      return {Pfx_NIL, false};
    auto [It, IsNewPfx] = PrefixMap.try_emplace(Pfx);
    return {&*It, IsNewPfx};
  }

  EXI_INLINE const InlineStr* internLocalName(StrRef Raw) {
    return NameCache.saveRaw(Raw);
  }
  /// Insert LocalName when it is known to not exist.
  LocalNameInfo* createNewLocalName(URIEntry* URI, StrRef Raw);
  /// Get LocalName if it exists, otherwise create new entry.
  std::pair<LocalNameInfo*, bool> getLocalName(URIEntry* URI, StrRef Raw);

  ////////////////////////////////////////////////////////////////////////
  // Batch Initialization

  /// Creates the initial entries for the string table. The values inserted
  /// depend on the schema.
  void createInitialEntries(bool UsesSchema);
  /// Appends LocalNames to the provided URI.
  void appendLocalNames(URIEntry* ID, ArrayRef<StrRef> LNMappings);
};

bool PrefixInfo::isSyncedWithURI() const {
  if EXI_NEVER(Link == nullptr)
    Throw<uninit_error>("Prefix::Link is uninitialized!");
  auto* UI = StringTable::VOfX(Link);
  return WithURI == UI->uri()
      && PfxLogQ == UI->pfxLogQ()
      && UI->contains(this);
}

void PrefixInfo::syncWithURI() {
  if EXI_NEVER(Link == nullptr)
    Throw<uninit_error>("Prefix::Link is uninitialized!");
  PfxLogQ = StringTable::VOfX(Link)->pfxLogQ();
}

inline unsigned LocalNameInfo::CheckURI(STURIEntry* ID) {
  if EXI_NEVER(!ID)
    Throw<argument_error>("LocalName URI is null!");
  return StringTable::VOfX(ID)->uri();
}
inline unsigned LocalNameInfo::GetLNID(STURIEntry* ID) {
  exi_invariant(ID != nullptr);
  return StringTable::VOfX(ID)->LocalNames++;
}

inline LocalNameInfo::LocalNameInfo(STURIEntry* ID, const InlineStr* LocalName) :
 URI(ID), WithURI(CheckURI(ID)), LNID(GetLNID(ID)), LocalName(LocalName) {
  if EXI_NEVER(!LocalName)
    Throw<argument_error>("LocalName Name is null!");
}

inline StrRef LocalNameInfo::uriName() const {
  return StringTable::GetURI(URI);
}

} // namespace encode
} // namespace exi

#undef DECL_MAPPING_X
#undef DECL_UNMAPPING
#undef DECL_MAPPINGS_X
#undef DECL_MAPPINGS
#undef DECL_VOF
