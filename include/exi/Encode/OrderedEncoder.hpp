//===- exi/Encode/OrderedEncoder.hpp --------------------------------===//
//
// Copyright (C) 2025-2026 Ninefold
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
/// This file implements an exi processor for in-order writers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Unwrap.hpp>
#include <exi/Basic/XML.hpp>
#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/StringTable.hpp>
#include <exi/Encode/NamespaceContextStack.hpp>
#include <exi/Encode/BodyEncoderAlloc.hpp>
#include <exi/Grammar/EncoderSchema.hpp>
#include <exi/Stream/OrderedWriter.hpp>
#include <exi/Basic/D/LogPosition.mac>

#define DEBUG_TYPE "OrderedEncoder"

namespace exi {

class OrderedEncoder final : public BodyEncoder {
  friend class ExiEncoder;
  friend struct encode::Schema::Get<BitWriter>;
  friend struct encode::Schema::Get<ByteWriter>;

  using BodyEncoder::Opts;
  /// A BumpPtrAllocator for processor internals.
  mutable EncoderBumpAllocator BP;
  /// The provided `OrderedWriter`.
  OrdWriter Writer;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  encode::StringTable Strings;
  /// The stack of namespace contexts.
  NSContextStack CtxStack;
  /// The schema for the current document.
  Box<encode::Schema> CurrentSchema;

  bool IsConstructed : 1 = false;
  bool IsStreamInitialized : 1 = false;
  bool DidEncodeHeader : 1 = false;

  /// Creates a new schema instance if initialized.
  Box<encode::Schema> makeSchemaFromThis(encode::factory_t& F) {
    if EXI_UNLIKELY(!F)
      return nullptr;
    return F(this);
  }

public:
  OrderedEncoder(ExiOptions& Opts, encode::factory_t& F, ExiError* E = nullptr);
  /// Initializes the writer with a stream/buffer.
  template <typename InitT>
  OrderedEncoder(ExiOptions& Opts, encode::factory_t& F,
    InitT& I, ExiError* E = nullptr)
   : OrderedEncoder(Opts, F, E) {
    if (auto Err = init(I)) [[unlikely]]
      if (E != nullptr)
        *E = std::move(Err);
  }

  EncoderBumpAllocator& getAllocator() const { return BP; }

  void* Allocate(usize Size, unsigned Align = 8) const {
    return BP.Allocate(Size, Align);
  }
  template <typename T> T* Allocate(usize N = 1) const {
    return static_cast<T*>(Allocate(N * sizeof(T), alignof(T)));
  }
  void Deallocate(void* Ptr) const {}

  EXI_FLATTEN ALWAYS_INLINE encode::Schema* getSchema() const {
    return CurrentSchema.operator->();
  }

  void flush() override {
    Writer->flush();
  }
  
  StreamBase::StreamKind streamKind() const override {
    return Writer->getStreamKind();
  }

  static bool classof(const BodyEncoder* BE) {
    return BE->kind() == EncoderKind::EK_Ordered;
  }

  EXI_FLATTEN usize bitPos() const {
    return Writer->bitPos();
  }

  usize depth() const {
    return CtxStack.total_depth();
  }

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Returns an error if the writer isn't empty.
  ExiError assumeWriterIsEmpty() const;
  /// Generic interface for initializing the OrderedWriter.
  ExiError init(raw_ostream& Strm);
  /// Generic interface for initializing the OrderedWriter.
  ExiError init(SmallVecImpl<char>& Buf);

private:
  /// Initializes the writer with arbitrary data.
  void initWriter(auto& BufOrStrm, auto...Rest) {
    if (Opts.Alignment == AlignKind::BitPacked) {
      Writer.emplace<BitWriter>(BufOrStrm, Rest...);
    } else /*AlignKind::BytePacked*/ {
      Writer.emplace<ByteWriter>(BufOrStrm, Rest...);
    }
    IsStreamInitialized = true;
  }

  template <typename StrmT = OrderedWriter>
  EXI_INLINE StrmT& writer() {
    if constexpr (std::same_as<StrmT, OrderedWriter>)
      return *Writer;
    else
      return cast<StrmT>(Writer);
  }

public:
  /// Checks that a `BitBuffer` could contain valid data.
  inline static bool IsValidHeaderBuffer(BitBuffer Data,
                                         AlignKind A = AlignKind::BitPacked);
  /// Checks if the data should be written to the buffer.
  /// @returns A value if no, otherwise `None`.
  inline Option<ExiError> shouldEncodeHeader(BitBuffer Data,
                                             bool KnownValid = false) const;
  /// Writes the header to the provided stream.
  inline ExiError encodeHeader(BitBuffer Data, bool KnownValid = false);
  /// Writes the header to the provided stream.
  ExiError encodeHeader(BitBuffer Data) override;

  bool isReady() const override {
    return IsConstructed
        && IsStreamInitialized
        && DidEncodeHeader;
  }

  ////////////////////////////////////////////////////////////////////////
  // Interface

  ExiError StartDocument() {
    static constexpr StartDocEvent SD
      = make_event<SimpleEventTerm::SD>();
    exi_assert(CtxStack.total_depth() == 0);
    return getSchema()->encode(this, SD);
  }
  ExiError EndDocument() {
    static constexpr EndDocEvent ED
      = make_event<SimpleEventTerm::ED>();
    exi_assert(CtxStack.total_depth() == 0);
    return getSchema()->encode(this, ED);
  }

  ExiError StartElement(StrRef Name) {
    CtxStack.inc();
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::SE>(Name));
  }
  ExiError StartElementURI(StrRef Name, StrRef URI) {
    CtxStack.inc();
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::SE>(Name, URI));
  }
  ExiError StartElementURI(StrRef Name, encode::STURIEntry* URI) {
    CtxStack.inc();
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::SE>(Name, URI));
  }
  ExiError EndElement() {
    static constexpr EndElemEvent EE
      = make_event<SimpleEventTerm::EE>();
    CtxStack.pop(Strings);
    return getSchema()->encode(this, EE);
  }

  ExiError Attribute(StrRef Name, StrRef Value) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::AT>(Name, Value));
  }
  ExiError BatchAttribute(ArrayRef<AttrEvent> Arr) {
    return getSchema()->batchEncode(this, Arr);
  }

  ExiError Characters(StrRef Value) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::CH>(Value));
  }

  template <bool IsRoot>
  ExiError Namespace(StrRef Pfx, StrRef URI, bool IsLocal = false) {
    if constexpr (!IsRoot)
      return getSchema()->encode(this,
        make_event<SimpleEventTerm::NS>(Pfx, URI, IsLocal));
    else
      return BatchNamespace<IsRoot>(
        make_event<SimpleEventTerm::NS>(Pfx, URI, IsLocal));
  }
  template <bool IsRoot>
  ExiError BatchNamespace(ArrayRef<NamespaceEvent> Arr) {
    return getSchema()->batchEncode<IsRoot>(this, Arr);
  }

  ExiError Comment(StrRef Data) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::CM>(Data));
  }
  ExiError ProcessingInstruction(StrRef Name, StrRef Data) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::PI>(Name, Data));
  }
  ExiError Doctype(const DoctypeEvent& DocType) {
    return getSchema()->encode(this, DocType);
  }

  ////////////////////////////////////////////////////////////////////////
  // Terms

  using URIEntry    = encode::STURIEntry;
  using PrefixEntry = encode::STPrefixEntry;
  using NameEntry   = encode::LocalNameInfo;
  using ValueEntry  = encode::STValueEntry;

  /// Represents a [Prefix, IsNew] pair
  using TaggedPrefixEntry = std::pair<PrefixEntry*, bool>;

  using BodyEncoder::SplitName;

  static StrRef GetSEUriValue(const StartElemURIEvent& SE) {
    if EXI_LIKELY(SE.tag() == StringEventKind::URI)
      return StrRef(SE.URI, SE.Extra);
    else
      return encode::StringTable::GetURI(SE.OpaqueURI);
  }

  /// Checks for a [URI, LocalName] pair given an SE(*) event.
  std::pair<URIEntry*, NameEntry*> lookupSE(const StartElemEvent& SE) {
    auto [Pfx, LN] = SplitName(SE.name());
    URIEntry* URIV = Strings.getURIFromPfx(Pfx);
    if (!URIV)
      return {nullptr, nullptr};
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }
  // Checks for a [URI, LocalName] pair given an SE(uri:*) event.
  std::pair<URIEntry*, NameEntry*> lookupSEUri(const StartElemURIEvent& SE) {
    URIEntry* URIV = Strings.lookupURI(GetSEUriValue(SE));
    if EXI_UNLIKELY(!URIV)
      return {nullptr, nullptr};
    auto [Pfx, LN] = SplitName(SE.name());
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }
  std::pair<URIEntry*, NameEntry*> lookupAT(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    URIEntry* URIV = Strings.getURIFromPfx</*IsAT=*/true>(Pfx);
    if (!URIV)
      return {nullptr, nullptr};
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }

  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT>
  ExiResult<NameEntry*> encodeSE(const StartElemEvent& SE) {
    return encodeQName<StrmT>(SE.name());
  }
  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT>
  ExiResult<NameEntry*> encodeSEUri(const StartElemURIEvent& SE) {
    return encodeLateBoundQName<StrmT>(SE.name(), GetSEUriValue(SE));
  }

  template <typename StrmT>
  ExiResult<NameEntry*> encodeAT(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    encode::LocalNameInfo* LNV
      = EXI_UNWRAP((encodeQName<StrmT, /*IsAT=*/true>(Pfx, LN)));
    exi_guard_invariant(LNV != nullptr);
    exi_try_r(encodeValue<StrmT>(LNV, AT[1]));
    return LNV;
  }
  template <typename StrmT>
  ExiError encodeATKnown(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    encode::LocalNameInfo* LNV
      = EXI_UNWRAP((onlyGetKnownQName<StrmT, /*IsAT=*/true>(Pfx, LN)));
    exi_guard_invariant(LNV != nullptr);
    return encodeValue<StrmT>(LNV, AT[1]);
  }

  // FIXME: Handle popping scope after inner ns decl?
  // TODO: Add option to keep/prune useless ns events

  template <typename StrmT, bool IsRoot = false>
  ExiError encodeNS(const NamespaceEvent& NS) {
    StrRef URI(NS.UriData, NS.UriSize);
    URIEntry* URIV = encodeURI<StrmT>(URI);
    exi_assert(URIV != nullptr);
    StrRef Pfx(NS.PfxData, NS.PfxSize);
    ExiResult<TaggedPrefixEntry> PfxInfoOrErr = encodePfx<StrmT>(URIV, Pfx);
    exi_try_unwrap(PfxInfoOrErr);

    if constexpr (!IsRoot)
      if (!PfxInfoOrErr->second)
        CtxStack.add(PfxInfoOrErr->first);
    
    LOG_POSITION(this);
    LOG_INFO(">> {}", NS.IsLocal ? "LOCAL" : "NON-LOCAL");
    writer<StrmT>().writeBit(NS.IsLocal);
    return ExiError::OK;
  }
  /// Handles the case of a pseudo-NS event with Preserve.Prefixes off.
  template <bool IsRoot = false>
  ExiError saveNSToTableOnly(const NamespaceEvent& NS) {
    StrRef Pfx(NS.PfxData, NS.PfxSize);
    StrRef URI(NS.UriData, NS.UriSize);
    TaggedPrefixEntry PfxInfo = Strings.enterNamespaceFacade(Pfx, URI);
    if constexpr (!IsRoot)
      if (!PfxInfo.second)
        CtxStack.add(PfxInfo.first);
    return ExiError::OK;
  }

  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT, bool IsAT = false>
  ExiResult<NameEntry*> encodeQName(StrRef Name) {
    auto [Pfx, LN] = SplitName(Name);
    return encodeQName<StrmT, IsAT>(Pfx, LN);
  }
  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT, bool IsAT = false>
  ExiResult<NameEntry*> encodeQName(StrRef Pfx, StrRef LN) {
    PrefixEntry* PfxV = Strings.lookupPfx<IsAT>(Pfx);
    if EXI_NEVER(PfxV == nullptr) {
      LOG_ERROR("No prefix '{}'", Pfx);
      return Err(ErrorCode::kNullptrRef);
    }
    URIEntry* URIV = Strings.GetURIEntry(PfxV);
    if EXI_NEVER(URIV == nullptr) {
      LOG_ERROR("No URI bound to prefix '{}'", Pfx);
      return Err(ErrorCode::kNullptrRef);
    }
    (void) encodeURIID<StrmT>(URIV);
    NameEntry* LNV = EXI_UNWRAP(encodeName<StrmT>(URIV, LN));
    exi_try_r(encodePfxQ<StrmT>(URIV, PfxV));
    return LNV;
  }
  /// Encodes a `*:local-name` with a late-bound prefix mapping.
  template <typename StrmT>
  ExiResult<NameEntry*> encodeLateBoundQName(StrRef Name, StrRef URI) {
    auto [Pfx, LN] = SplitName(Name);
    URIEntry* URIV = encodeURI<StrmT>(URI);
    if EXI_NEVER(URIV == nullptr) {
      LOG_ERROR("No URI could be bound to prefix '{}'", Pfx);
      return Err(ErrorCode::kUnexpectedError);
    }

    NameEntry* LNV = EXI_UNWRAP(encodeName<StrmT>(URIV, LN));
    exi_try_r(encodePfxQ<StrmT>(URIV, Pfx));
    return LNV;
  }
  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT, bool IsAT = false>
  ExiResult<NameEntry*> onlyGetKnownQName(StrRef Pfx, StrRef LN) {
    PrefixEntry* PfxV = Strings.lookupPfx<IsAT>(Pfx);
    if EXI_NEVER(PfxV == nullptr) {
      LOG_ERROR("No prefix '{}'", Pfx);
      return Err(ErrorCode::kNullptrRef);
    }
    URIEntry* URIV = Strings.GetURIEntry(PfxV);
    if EXI_NEVER(URIV == nullptr) {
      LOG_ERROR("No URI bound to prefix '{}'", Pfx);
      return Err(ErrorCode::kNullptrRef);
    }
    Result LNOrIP = Strings.lookupLocalName(URIV, LN);
    return LNOrIP.expect("LN should exist for an SE/AT(qname) event.");
  }

  /// Encodes a uri.
  template <typename StrmT>
  URIEntry* encodeURI(StrRef URI) {
    if (auto* URIV = Strings.lookupURI(URI))
      return encodeURIID<StrmT>(URIV);
    return encodeURIStr<StrmT>(URI);
  }
  /// Encodes a given `URIEntry`.
  template <typename StrmT>
  URIEntry* encodeURIID(URIEntry* URI) {
    LOG_POSITION(this);
    auto [ID, Bits] = Strings.getURIIDAndLog(URI);
    writer<StrmT>().writeBits64(ID + 1, Bits);
    LOG_INFO(">> URI(Hit) @{}: \"{}\"", ID, Strings.GetURI(URI));
    return URI;
  }
  /// Encodes a uri string.
  template <typename StrmT>
  URIEntry* encodeURIStr(StrRef URI) {
    LOG_POSITION(this);
    u32 Bits = Strings.getURILog();
    writer<StrmT>().writeBits64(0, Bits);
    writer<StrmT>().encodeString(URI);
    URIEntry* URIV = Strings.addURI(URI);
    LOG_INFO(">> URI(Miss) @{}: \"{}\"", Strings.GetID(URIV), URI);
    return URIV;
  }

  template <typename StrmT>
  ExiResult<NameEntry*> encodeName(URIEntry* URI, StrRef LN) {
    Result LNOrIP = Strings.lookupLocalName(URI, LN);
    if (LNOrIP.is_ok())
      return encodeName<StrmT>(URI, *LNOrIP);
    encode::LocalNameInsert* IP = LNOrIP.error();
    if EXI_NEVER(!IP) {
      LOG_ERROR("InsertionPoint cannot be null!");
      return Err(ErrorCode::kNullptrRef);
    }
    LOG_POSITION(this);
    encodeUInt<StrmT>(countRunes(LN) + 1);
    LOG_POSITION(this);
    writer<StrmT>().writeString(LN);
    NameEntry* LNV = Strings.addLocalName(URI, LN, IP);
    LOG_INFO(">> LN @{}: \"{}\"", LNV->id(), LN);
    return LNV;
  }
  template <typename StrmT>
  ExiResult<NameEntry*> encodeName(URIEntry* URI, NameEntry* LN) {
    LOG_POSITION(this);
    u32 Bits = Strings.getLocalNameLog(URI);
    encodeUInt<StrmT>(0);
    LOG_POSITION(this);
    writer<StrmT>().writeBits64(LN->id(), Bits);
    LOG_INFO(">> LN @{}: \"{}\"", LN->id(), LN->name());
    return LN;
  }

  /// Encodes a QName prefix.
  template <typename StrmT>
  ExiError encodePfxQ(URIEntry* URI, StrRef Pfx) {
    if (shouldPfxQEarlyReturn(URI))
      return ExiError::OK;
    // By this point we know the URI isn't null.
    if (auto* PfxV = Strings.lookupPfxForURI(URI, Pfx))
      return encodePfxQID<StrmT>(URI, PfxV);
    // Check if encoding is even required.
    if (usize N = Strings.GetPfxCount(URI); N > 1) {
      LOG_POSITION(this);
      unsigned Bits = ID_Log2</*NeverZero=*/true>(N);
      writer<StrmT>().writeBits64(0, Bits);
    }
    LOG_INFO(">> PXQ (Miss)");
    return ExiError::OK;
  }
  /// Encodes a QName with a known prefix.
  template <typename StrmT>
  ExiError encodePfxQ(URIEntry* URI, PrefixEntry* Pfx) {
    if (shouldPfxQEarlyReturn(URI, Pfx))
      return ExiError::OK;
    tail_return encodePfxQID<StrmT>(URI, Pfx);
  }
  template <typename StrmT>
  ExiError encodePfxQID(URIEntry* URI, PrefixEntry* Pfx) {
    exi_invariant(URI && Pfx && this->PreservePrefixes());
    auto [ID, Bits] = Strings.getPfxIDAndLogQ(Pfx);
    if (Bits) {
      LOG_POSITION(this);
      writer<StrmT>().writeBits64(ID, Bits);
    }
    LOG_INFO(">> PXQ (Hit) @{}: \"{}\"", ID, Strings.GetPfx(Pfx));
    return ExiError::OK;
  }
  /// Checks the conditions for returning early for the URI and Prefix.
  EXI_INLINE bool shouldPfxQEarlyReturn(URIEntry* URI, PrefixEntry* Pfx) {
    if (shouldPfxQEarlyReturn(URI))
      return true;
    if (!Pfx) {
      LOG_WARN(">> PXQ (null)");
      return true;
    }
    return false;
  }
  /// Checks the conditions for returning early solely for the URI.
  EXI_INLINE bool shouldPfxQEarlyReturn(URIEntry* URI) {
    exi_invariant(URI != nullptr,
                 "URI should be validated before this point!");
    if (!this->PreservePrefixes())
      return true;
    if (Strings.GetID(URI) == 0) {
      // Skip encoding the null URI ("").
      LOG_INFO(">> PXQ (Nil) @*: \"\"");
      return true;
    }
    return false;
  }

  /// Encodes a NS event prefix.
  template <typename StrmT>
  ExiResult<TaggedPrefixEntry> encodePfx(URIEntry* URI, StrRef Pfx) {
#if EXI_DEBUG
    if (!this->PreservePrefixes()) {
      LOG_ERROR("Encoded NS prefix with prefixes disabled!");
      return Err(ErrorCode::kInconsistentProcState);
    }
#endif
    if (PrefixEntry* PfxV = Strings.lookupPfxForURI(URI, Pfx)) {
      (void) encodePfxID<StrmT>(URI, PfxV);
      /// TODO: Verify this...
      return TaggedPrefixEntry{PfxV, false};
    }
    LOG_POSITION(this);
    unsigned Bits = Strings.getPfxLog(URI);
    writer<StrmT>().writeBits64(0, Bits);
    writer<StrmT>().encodeString(Pfx);
    auto [PfxV, IsNew] = Strings.addPrefix(URI, Pfx);
    LOG_INFO(">> PXNS (Miss) @{}: \"{}\"", Strings.GetID(PfxV), Pfx);
    return TaggedPrefixEntry{PfxV, IsNew};
  }
  template <typename StrmT>
  PrefixEntry* encodePfxID(URIEntry* URI, PrefixEntry* Pfx) {
    Strings.enterKnownNamespace(URI, Pfx);
    if (unsigned Bits = Strings.getPfxLog(Pfx)) {
      LOG_POSITION(this);
      unsigned ID = Strings.GetID(Pfx);
      writer<StrmT>().writeBits64(ID + 1, Bits);
      LOG_INFO(">> PXNS (Hit) @{}: \"{}\"", ID, Strings.GetPfx(Pfx));
      return Pfx;
    }
    LOG_INFO(">> PXNS (Hit) @0: \"{}\"", Strings.GetPfx(Pfx));
    return Pfx;
  }

  template <typename StrmT>
  ExiError encodeValue(NameEntry* LN, StrRef Value) {
    LOG_POSITION(this);
    CachedHashStrRef ChValue = Strings.prehash(Value);
    auto [GV, IsLocal] = Strings.lookupLocalValue(LN, ChValue);
    if (IsLocal && GV) /*TODO: Assert GV*/ {
      encodeUInt<StrmT>(0);
      unsigned Bits = LN->log();
      unsigned ID = Strings.GetLocalID(GV);
      LOG_EXTRA("Encoding <{}>", Bits);
      writer<StrmT>().writeBits64(ID, Bits);
      LOG_INFO(">> LV (hit) @[{}:{}]:{}: \"{}\"",
        LN->uri(), LN->id(), ID, Value);
    } else if (GV) {
      encodeUInt<StrmT>(1);
      unsigned Bits = Strings.getGlobalValueLog();
      unsigned ID = Strings.GetGlobalID(GV);
      LOG_EXTRA("Encoding <{}>", Bits);
      writer<StrmT>().writeBits64(ID, Bits);
      LOG_INFO(">> GV (hit) @{}: \"{}\"", ID, Value);
    } else {
      encodeUInt<StrmT>(countRunes(Value) + 2);
      writer<StrmT>().writeString(Value);
      GV = Strings.addValue(LN, ChValue);
#if EXI_LOGGING
      unsigned ID = Strings.GetLocalID(GV);
      LOG_INFO(">> LV (miss) @[{}:{}]:{}: \"{}\"",
        LN->uri(), LN->id(), ID, Value);
#endif
    }
    return ExiError::OK;
  }

  // Simply writes a UInt with logging.
  template <typename StrmT>
  EXI_INLINE void encodeUInt(u64 Value) {
    LOG_EXTRA("Encoding UInt");
    LOG_EXTRA(">>> UInt {}", Value);
    writer<StrmT>().writeUInt(Value);
  }
};

} // namespace exi

//////////////////////////////////////////////////////////////////////////
// operator new/delete

/// Placement new for using OrderedEncoder's allocator.
///
/// This placement form of operator new uses the OrderedEncoder's allocator for
/// obtaining memory.
///
/// We intentionally avoid using a nothrow specification here so that the calls
/// to this operator will not perform a null check on the result -- the
/// underlying allocator never returns null pointers.
///
/// Memory allocated through this placement new operator does not need to be
/// explicitly freed, as OrderedEncoder will free all of this memory when it
/// gets destroyed. Please note that you cannot use delete on the pointer.
inline void* operator new(usize Bytes, const exi::OrderedEncoder& OE,
                          usize Alignment /* = 8 */) {
  return OE.Allocate(Bytes, Alignment);
}

/// Placement delete companion to the new above.
///
/// This operator is just a companion to the new above. There is no way of
/// invoking it directly; see the new operator for more details. This operator
/// is called implicitly by the compiler if a placement new expression using
/// the OrderedEncoder throws in the object constructor.
inline void operator delete(void* Ptr, const exi::OrderedEncoder& OE, usize) {
  OE.Deallocate(Ptr);
}

/// This placement form of operator new[] uses the OrderedEncoder's allocator
/// for obtaining memory.
///
/// We intentionally avoid using a nothrow specification here so that the calls
/// to this operator will not perform a null check on the result -- the
/// underlying allocator never returns null pointers.
///
/// Memory allocated through this placement new[] operator does not need to be
/// explicitly freed, as OrderedEncoder will free all of this memory when it
/// gets destroyed. Please note that you cannot use delete on the pointer.
inline void* operator new[](usize Bytes, const exi::OrderedEncoder& OE,
                            usize Alignment /* = 8 */) {
  return OE.Allocate(Bytes, Alignment);
}

/// Placement delete[] companion to the new[] above.
///
/// This operator is just a companion to the new[] above. There is no way of
/// invoking it directly; see the new[] operator for more details. This operator
/// is called implicitly by the compiler if a placement new[] expression using
/// the OrderedEncoder throws in the object constructor.
inline void operator delete[](void* Ptr, const exi::OrderedEncoder& OE, usize) {
  OE.Deallocate(Ptr);
}

#undef DEBUG_TYPE
#undef LOG_POSITION
