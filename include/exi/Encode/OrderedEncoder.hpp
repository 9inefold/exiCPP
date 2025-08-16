//===- exi/Encode/OrderedEncoder.hpp --------------------------------===//
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

  StreamBase::StreamKind streamKind() const override {
    return Writer->getStreamKind();
  }

  static bool classof(const BodyEncoder* BE) {
    return BE->kind() == EncoderKind::EK_Ordered;
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
    return getSchema()->encode(this, SD);
  }
  ExiError EndDocument() {
    static constexpr EndDocEvent ED
      = make_event<SimpleEventTerm::ED>();
    return getSchema()->encode(this, ED);
  }

  ExiError StartElement(StrRef Name) {
    CtxStack.inc(Strings);
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::SE>(Name));
  }
  ExiError StartElementURI(StrRef Name, StrRef URI) {
    CtxStack.inc(Strings);
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::SE>(Name, URI));
  }
  ExiError StartElementURI(StrRef Name, encode::STURIEntry* URI) {
    CtxStack.inc(Strings);
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

  [[nodiscard]] static std::pair<StrRef, StrRef> SplitName(StrRef S) {
    usize Idx = S.find(':');
    if (Idx == StrRef::npos)
      return std::make_pair(StrRef(), S);
    return std::make_pair(S.slice(0, Idx), S.substr(Idx + 1));
  }
  static StrRef GetSEUriValue(const StartElemURIEvent& SE) {
    if EXI_LIKELY(SE.Tag == 1)
      return StrRef(SE.URI, SE.Extra);
    else
      return encode::StringTable::GetURI(SE.OpaqueURI);
  }

  std::pair<URIEntry*, NameEntry*> lookupSE(const StartElemEvent& SE) {
    auto [Pfx, LN] = SplitName(SE.name());
    URIEntry* URIV = Strings.getURIFromPfx(Pfx);
    if (!URIV)
      return {nullptr, nullptr};
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }
  std::pair<URIEntry*, NameEntry*> lookupSEUri(const StartElemURIEvent& SE) {
    URIEntry* URIV = Strings.lookupURI(GetSEUriValue(SE));
    if (!URIV)
      return {nullptr, nullptr};
    auto [Pfx, LN] = SplitName(SE.name());
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }
  std::pair<URIEntry*, NameEntry*> lookupAT(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    URIEntry* URIV = Strings.getURIFromPfx(Pfx);
    if (!URIV)
      return {nullptr, nullptr};
    return {URIV, Strings.lookupLocalName(URIV, LN).value_or(nullptr)};
  }

  template <typename StrmT>
  ExiResult<NameEntry*> encodeSE(const StartElemEvent& SE) {
    return encodeQName<StrmT>(SE.name());
  }
  template <typename StrmT>
  ExiResult<NameEntry*> encodeSEUri(const StartElemURIEvent& SE) {
    return encodeLateBoundQName<StrmT>(SE.name(), GetSEUriValue(SE));
  }

  template <typename StrmT>
  ExiResult<NameEntry*> encodeAT(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    encode::LocalNameInfo* LNV
      = EXI_UNWRAP(encodeQName<StrmT>(Pfx, LN));
    exi_guard_invariant(LNV != nullptr);
    exi_try_r(encodeValue<StrmT>(LNV, AT[1]));
    return LNV;
  }
  template <typename StrmT>
  ExiError encodeATKnown(const AttrEvent& AT) {
    auto [Pfx, LN] = SplitName(AT[0]);
    encode::LocalNameInfo* LNV
      = EXI_UNWRAP(encodeQName<StrmT>(Pfx, LN));
    exi_guard_invariant(LNV != nullptr);
    return encodeValue<StrmT>(LNV, AT[1]);
  }

  template <typename StrmT, bool IsRoot = false>
  ExiError encodeNS(const NamespaceEvent& NS) {
    StrRef URI(NS.UriData, NS.UriSize);
    URIEntry* URIV = encodeURI<StrmT>(URI);
    exi_assert(URIV != nullptr);
    StrRef Pfx(NS.PfxData, NS.PfxSize);
    Result PfxOrErr = encodePfx<StrmT>(URIV, Pfx);
    exi_try_unwrap(PfxOrErr);
    if constexpr (!IsRoot)
      CtxStack.add(Strings, *PfxOrErr);
    writer<StrmT>().writeBit(NS.IsLocal);
    return ExiError::OK;
  }
  template <bool IsRoot = false>
  ExiError saveNSToTableOnly(const NamespaceEvent& NS) {
    exi_todo("Encoding without Preserve.Prefixes");
  }

  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT>
  ExiResult<NameEntry*> encodeQName(StrRef Name) {
    auto [Pfx, LN] = SplitName(Name);
    return encodeQName<StrmT>(Pfx, LN);
  }
  /// Encodes a `pfx?:local-name` with a predefined prefix-uri mapping.
  template <typename StrmT>
  ExiResult<NameEntry*> encodeQName(StrRef Pfx, StrRef LN) {
    PrefixEntry* PfxV = Strings.lookupPfx(Pfx);
    URIEntry* URIV = Strings.GetURIEntry(PfxV);
    if EXI_NEVER(URIV == nullptr) {
      LOG_ERROR("No URI bound to prefix '{}'", Pfx);
      return Err(ErrorCode::kNullptrRef);
    }
    (void) encodeURI<StrmT>(URIV);
    NameEntry* LNV = EXI_UNWRAP(encodeName<StrmT>(URIV, LN));
    exi_try_r(encodePfxQ<StrmT>(PfxV));
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
    if (auto* PfxV = Strings.GetAnyPfx(URIV))
      exi_try_r(encodePfxQ<StrmT>(PfxV));
    return LNV;
  }

  /// Encodes a uri.
  template <typename StrmT>
  URIEntry* encodeURI(StrRef URI) {
    if (auto* URIV = Strings.lookupURI(URI))
      return encodeURI<StrmT>(URIV);
    return encodeURIStr<StrmT>(URI);
  }
  /// Encodes a given `URIEntry`.
  template <typename StrmT>
  URIEntry* encodeURI(URIEntry* URI) {
    auto [ID, Bits] = Strings.getURIIDAndLog(URI);
    writer<StrmT>().writeBits64(ID + 1, Bits);
    return URI;
  }
  /// Encodes a uri string.
  template <typename StrmT>
  URIEntry* encodeURIStr(StrRef URI) {
    u32 Bits = Strings.getURILog();
    writer<StrmT>().writeBits64(0, Bits);
    writer<StrmT>().encodeString(URI);
    return Strings.addURI(URI);
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
    writer<StrmT>().writeUInt(LN.size() + 1);
    writer<StrmT>().writeString(LN);
    return Strings.addLocalName(URI, LN, IP);
  }
  template <typename StrmT>
  ExiResult<NameEntry*> encodeName(URIEntry* URI, NameEntry* LN) {
    u32 Bits = Strings.getLocalNameLog(URI);
    writer<StrmT>().writeBits64(LN->id(), Bits);
    return LN;
  }

  template <typename StrmT>
  ExiError encodePfxQ(PrefixEntry* Pfx) {
    if (!this->PreservePrefixes())
      return ExiError::OK;
    if EXI_UNLIKELY(!Pfx) {
      LOG_WARN("Pfx is null.");
      return ExiError::OK;
    }
    auto [ID, Bits] = Strings.getPfxIDAndLogQ(Pfx);
    writer<StrmT>().writeBits64(ID, Bits);
    return ExiError::OK;
  }

  template <typename StrmT>
  ExiResult<PrefixEntry*> encodePfx(URIEntry* URI, StrRef Pfx) {
    CachedHashStrRef ChPfx = Strings.prehash(Pfx);
#if EXI_DEBUG
    if (!this->PreservePrefixes()) {
      LOG_ERROR("Encoded NS prefix with prefixes disabled!");
      return Err(ErrorCode::kInconsistentProcState);
    }
#endif
    if (PrefixEntry* PfxV = Strings.lookupPfx(ChPfx))
      return encodePfx<StrmT>(PfxV);
    writer<StrmT>().encodeString(Pfx);
    return Strings.addPrefix(URI, ChPfx);
  }
  template <typename StrmT>
  PrefixEntry* encodePfx(PrefixEntry* Pfx) {
    if (unsigned Bits = Strings.getPfxLog(Pfx)) {
      unsigned ID = Strings.GetID(Pfx);
      writer<StrmT>().writeBits64(ID + 1, Bits);
    }
    return Pfx;
  }

  template <typename StrmT>
  ExiError encodeValue(NameEntry* LN, StrRef Value) {
    CachedHashStrRef ChValue = Strings.prehash(Value);
    auto [GV, IsLocal] = Strings.lookupLocalValue(LN, ChValue);
    if (IsLocal && GV) /*TODO: Assert GV*/ {
      writer<StrmT>().writeUInt(0);
      unsigned Bits = LN->log();
      unsigned ID = Strings.GetLocalID(GV);
      writer<StrmT>().writeBits64(ID, Bits);
      LOG_INFO(">> LV (hit): @{} <{}>", ID, Bits);
    } else if (GV) {
      writer<StrmT>().writeUInt(1);
      unsigned Bits = Strings.getGlobalValueLog();
      unsigned ID = Strings.GetGlobalID(GV);
      writer<StrmT>().writeBits64(ID, Bits);
      Strings.addLocalValue(LN, GV);
      LOG_INFO(">> GV (hit): @{} <{}>", ID, Bits);
    } else {
      writer<StrmT>().writeUInt(Value.size() + 2);
      writer<StrmT>().writeString(Value);
      Strings.addValue(LN, ChValue);
      LOG_INFO(">> LV (miss)");
    }
    return ExiError::OK;
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
