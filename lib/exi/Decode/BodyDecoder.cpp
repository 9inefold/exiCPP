//===- exi/Decode/BodyDecoder.cpp -----------------------------------===//
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
/// This file implements decoding of the EXI body from a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Decode/BodyDecoder.hpp>
#include <core/Common/EnumArray.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Common/Unwrap.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/D/InternalMacros.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Decode/Deserializer.hpp>
#include <exi/Basic/D/LogPosition.mac>
#include <fmt/ranges.h>

#define DEBUG_TYPE "BodyDecoder"

#if EXI_LOG_POSITION
# define LOG_META(...) LOG_EXTRA(__VA_ARGS__)
#else
# define LOG_META(...) ((void)(0))
#endif

using namespace exi;
using namespace exi::decode;

namespace INTERNAL_NS(exi) {
/// A class that does nothing. Used for empty deserialization.
class INTERNAL_LINKAGE BIDeserializer final : public Deserializer {};
} // namespace INTERNAL_NS

ExiDecoder::ExiDecoder(MaybeBox<ExiOptions>&& Opts, ExiError* Err) {
  ExiError::AsOutParam EAO(Err);
  if (auto E = setOptions(std::move(Opts))) {
    if EXI_UNLIKELY(!Err)
      Throw<argument_error>("Invalid options configuration.");
    EAO = std::move(E);
  }
}

//ExiDecoder::ExiDecoder(ExiDecoder&& O) :
// Header(std::move(O.Header)), Reader(std::move(O.Reader)),
// BP(std::move(O.BP)), Strings(std::move(O.Strings)),
// CurrentSchema(std::move(O.CurrentSchema)),
// Flags(O.Flags), Preserve(O.Preserve) {
//  O.Flags.SetReader = false;
//  O.Flags.DidHeader = false;
//  O.Flags.DidInit   = false;
//}

//////////////////////////////////////////////////////////////////////////
// Initialization

//ExiResult<ExiDecoder> ExiDecoder::New(MaybeBox<ExiOptions>&& Opts) {
//  ExiError E = ExiError::OK;
//  ExiDecoder Out(std::move(Opts), &E);
//  if (E != ExiError::OK)
//    return Err(E);
//  return std::move(Out);
//}

ExiError ExiDecoder::assumeReaderIsUntouched() const {
  if (isReaderInitialized()) {
    LOG_ERROR("Invalid processor state!");
    return ErrorCode::kInvalidConfig;
  }
  return ExiError::OK;
}

ExiError ExiDecoder::setOptions(MaybeBox<ExiOptions>&& Opts) {
  if (!Opts) {
    LOG_WARN("Options are null, leave unset if not required.");
    // TODO: If permissive return OK.
    return ErrorCode::kNullptrRef;
  } else if (Flags.DidHeader) {
    // FIXME: Why am I even checking this
    exi_invariant(Header.Opts, "Options not initialized!");
    return assumeReaderIsUntouched();
  } else if (auto E = ValidateOptions(*Opts)) {
    return E;
  }

  Header.Opts = std::move(Opts);
  if (isReaderInitialized())
    // HACK: The assumption is the header doesn't need to be read.
    // This may not be true in all cases, so verify it.
    Flags.DidHeader = true;
  
  LOG_EXTRA("Options set manually.");
  return ExiError::OK;
}

ExiError ExiDecoder::setReader(UnifiedBuffer Buffer) {
  if (!Header.Opts) {
    // BUG: This error is actually incorrect, it doesn't matter what the initial
    // type of the reader is, it acts as a BitReader anyways.
    LOG_ERROR("Cannot deduce stream type without header.");
    return ErrorCode::kInvalidConfig;
  }

  if (Header.Opts->Alignment == AlignKind::BitPacked)
    Reader.emplace<BitReader>(Buffer.arr());
  else
    Reader.emplace<ByteReader>(Buffer.arr());

  Flags.DidHeader = true;
  Flags.SetReader = true;

  LOG_EXTRA("Reader set manually.");
  return ExiError::OK;
}

ExiError ExiDecoder::init() {
  if (Flags.DidInit) {
    exi_assert(Header.Opts);
    return assumeReaderIsUntouched();
  }

  if (!Header.Opts || Reader.empty()) {
    LOG_ERROR("Options or Reader are not initialized.");
    return ErrorCode::kInvalidConfig;
  }

  auto& Opts = *Header.Opts;
  if (!Opts.SchemaID.expect("schema is required"))
    CurrentSchema = BuiltinSchema::New(Opts);
  else
    exi_todo("schemas are currently unsupported");
  
  if (!CurrentSchema) {
    LOG_ERROR("Schema could not be allocated.");
    return ErrorCode::kInvalidMemoryAlloc;
  }

#if !defined(NDEBUG) || EXI_ENABLE_DUMP
  if (hasDbgLogLevel(INFO))
    CurrentSchema->dump();
#endif
  // TODO: Load schema
  Strings.setup(Opts);

  Preserve = Opts.Preserve;
  Flags.DidHeader = true;
  Flags.DidInit = true;

  LOG_EXTRA("Initialized!");
  return ExiError::OK;
}

ExiError ExiDecoder::prepareForDecoding() {
  if (!Flags.DidInit && !Flags.SetReader) {
    // No init because required options were not provided.
    if (!Flags.DidHeader)
      LOG_ERROR("Header has not been initialized.");
    LOG_ERROR("Initialization has not been completed.");
    return ErrorCode::kInvalidConfig;
  } else if (!Flags.DidInit) {
    // No init because external reader was provided.
    exi_assert(!Reader.empty());
    if (ExiError E = this->init())
      return E;
  }

  exi_invariant(Header.Opts && CurrentSchema);
  auto* SchemaPtr = CurrentSchema.get();

  if (!isa<BuiltinSchema>(*SchemaPtr)) {
    // TODO: Schemas!!!
    exi_unimplemented("dynamic and compiled schemas currently unsupported.");
  }

  return ExiError::OK;
}

ExiError ExiDecoder::decodeBody() {
  BIDeserializer S{};
  return this->decodeBody(&S);
}

template <>
EXI_USED ExiError ExiDecoder::decodeBody<>(Deserializer* S) {
  if (S == nullptr) {
    // TODO: Allow defaulting in permissive mode?
    LOG_ERROR("Deserializer cannot be null!");
    return ErrorCode::kInvalidEXIInput;
  }
  if (ExiError E = prepareForDecoding())
    return E;
  return decodeBodySwitchI<Deserializer>(S);
}

EXI_COLD ExiError ExiDecoder::dispatchUncommonEvent(Deserializer* S,
                                                    const EventUID Event) {
  switch (Event.getTerm()) {
  case EventTerm::SD:       // Start Document
    return S->SD();
  case EventTerm::ED:       // End Document
    if (ExiError E = S->ED())
      return E;
    return ExiError::DONE;
  case EventTerm::CM:       // Comment text (text)
    return this->handleCM(S);
  case EventTerm::PI:       // Processing Instruction (name, text)
    return this->handlePI(S);
  case EventTerm::DT:       // DOCTYPE (name, public, system, text)
    return this->handleDT(S);
  case EventTerm::ER:       // Entity Reference (name)
    return this->handleER(S);
  case EventTerm::SC:       // Self Contained
    return ErrorCode::kUnimplemented;
  default:
    exi_assert("unknown term");
    return ErrorCode::kInvalidEXIInput;
  }
}

//////////////////////////////////////////////////////////////////////////
// Terms

ExiError ExiDecoder::handleSD(Deserializer* S) {
  return S->SD();
}

ExiError ExiDecoder::handleED(Deserializer* S) {
  if (ExiError E = S->ED())
    return E;
  return ExiError::DONE;
}

#define READ_STRING(NAME, RESERVE, READER)                                    \
  SmallStr<RESERVE> NAME##_Data;                                              \
  Result NAME = (READER)->decodeString(NAME##_Data);                          \
  if EXI_UNLIKELY(NAME.is_err())                                              \
    return NAME.error();

ExiError ExiDecoder::handleCM(Deserializer* S) {
  READ_STRING(Comment, 80, Reader)
  if (S->needsPersistence())
    this->internStrings(*Comment);
  return S->CM(*Comment);
}

EXI_COLD ExiError ExiDecoder::handlePI(Deserializer* S) {
  return Reader.visit([this, S] (auto& Strm) -> ExiError {
    READ_STRING(Target, 16, &Strm)
    READ_STRING(Text,   48, &Strm)
    if (S->needsPersistence())
      this->internStrings(*Target, *Text);
    return S->PI(*Target, *Text);
  });
}

EXI_COLD ExiError ExiDecoder::handleDT(Deserializer* S) {
  return Reader.visit([this, S] (auto& Strm) -> ExiError {
    READ_STRING(Name,  16, &Strm)
    READ_STRING(PubID, 16, &Strm)
    READ_STRING(SysID, 16, &Strm)
    READ_STRING(Text,  32, &Strm)
    if (S->needsPersistence())
      this->internStrings(*Name, *PubID, *SysID, *Text);
    return S->DT(*Name, *PubID, *SysID, *Text);
  });
}

EXI_COLD ExiError ExiDecoder::handleER(Deserializer* S) {
  READ_STRING(Entity, 16, Reader)
  if (S->needsPersistence())
    this->internStrings(*Entity);
  return S->CM(*Entity);
}

//////////////////////////////////////////////////////////////////////////
// Util

QName ExiDecoder::getQName(EventUID Event) {
  exi_invariant(Event.hasQName());
  auto [URI, LocalName] = Strings.getQName(Event.Name);
  if (!Event.hasPrefix())
    return QName::Unbound(URI, LocalName, Event.getURI());
  /// Full name found.
  StrRef Pfx = Strings.getPrefix(
    Event.getURI(), Event.getPrefix());
  return QName::New(URI, LocalName, Pfx);
}

StrRef ExiDecoder::getPfxOrURI(EventUID Event) {
  if (!Event.hasQName())
    return "*"_str;
  
  const CompactID URI = Event.getURI();
  if (!Strings.hasPrefix(URI)) {
    LOG_META("No Prefix for @{}: {}", URI, Preserve.Prefixes);
    if (Strings.hasURI(URI))
      return Strings.getURI(URI);
    else
      return "?"_str;
  }
  
  if (Event.hasPrefix()) {
    const CompactID PfxID = Event.getPrefix();
    if (auto X = tryGetPfx(URI, PfxID)) {
      LOG_META("Prefix for @{}: {}", URI, PfxID);
      return *X;
    }
  }

  // Try the default if no match.
  if (auto X = tryGetPfx(URI, 0)) {
    LOG_META("Default Prefix for @{}", URI);
    return *X;
  }

  return Strings.getURI(URI);
}

Option<StrRef> ExiDecoder::tryGetPfx(CompactID URI, CompactID PfxID) {
  if (!Strings.hasPrefix(URI, PfxID))
    return std::nullopt;
  
  StrRef Pfx = Strings.getPrefix(URI, PfxID);
  if (Pfx.empty() && URI != 0) {
    // Don't allow arbitrary empty prefixes when printing.
    // May be confusing for the reader.
    return std::nullopt;
  }

  return Pfx;
}

//////////////////////////////////////////////////////////////////////////
// Values

template <typename StrmT>
ExiResult<EventUID> ExiDecoder::decodeQName() {
  const CompactID URI = EXI_UNWRAP(decodeURI<StrmT>());
  const CompactID LNI = EXI_UNWRAP(decodeName<StrmT>(URI));
  Option Pfx = EXI_UNWRAP(decodePfxQ<StrmT>(URI));

  auto QName = SmallQName::NewQName(URI, LNI);
  return Ok(EventUID::NewQName(QName, Pfx));
}

template <typename StrmT>
ExiResult<EventUID> ExiDecoder::decodeNS() {
  const CompactID URI = EXI_UNWRAP(decodeURI<StrmT>());
  const CompactID PfxID = EXI_UNWRAP(decodePfx<StrmT>(URI));

  bool IsLocal = false;
  LOG_POSITION(this);
  exi_try_r(reader<StrmT>().readBit(IsLocal));
  LOG_INFO(">> {}", IsLocal ? "LOCAL" : "NON-LOCAL");

  auto QName = SmallQName::NewURI(URI);
  return Ok(EventUID::NewNS(QName, PfxID, IsLocal));
}

template <typename StrmT>
ExiResult<CompactID> ExiDecoder::decodeURI() {
  CompactID URI; {
    const u64 NBits = Strings.getURILog();
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(reader<StrmT>().readBits64(URI, NBits));
  }

  if (URI == 0) {
    // Cache miss
    StrRef URIStr;
    SmallStr<32> Data;
    LOG_POSITION(this);
    StrRef Str = EXI_UNWRAP(reader<StrmT>().decodeString(Data));
    std::tie(URIStr, URI) = Strings.addURI(Str);
    LOG_INFO(">> URI(Miss) @{}: \"{}\"", URI, URIStr);
  } else {
    // Cache hit
    URI -= 1;
#if EXI_LOGGING
    StrRef URIStr = Strings.getURI(URI);
    LOG_INFO(">> URI(Hit) @{}: \"{}\"", URI, URIStr);
#endif
  }

  return URI;
}

template <typename StrmT>
ExiResult<CompactID> ExiDecoder::decodeName(CompactID URI) {
  CompactID LnID; {
    LOG_POSITION(this);
    LOG_EXTRA("Decoding UInt");
    exi_try_r(reader<StrmT>().readUInt(LnID));
    LOG_EXTRA(">>> UInt {}", LnID);
  }

  StrRef LocalName;
  if (LnID == 0) {
    // Cache hit
    const u64 NBits = Strings.getLocalNameLog(URI);
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(reader<StrmT>().readBits64(LnID, NBits));
#if EXI_LOGGING
    LocalName = Strings.getLocalName(URI, LnID);
#endif
  } else {
    // Cache miss
    LnID -= 1;
    SmallStr<32> Data;
    StrRef Str = EXI_UNWRAP(reader<StrmT>().readString(LnID, Data));
    std::tie(LocalName, LnID) = Strings.addLocalName(URI, Str);
  }

  LOG_INFO(">> LN @{}: \"{}\"", LnID, LocalName);
  return LnID;
}

template <typename StrmT>
ExiResult<Option<CompactID>> ExiDecoder::decodePfxQ(CompactID URI) {
  if (!Preserve.Prefixes)
    return Ok(std::nullopt);
  if (URI == 0) {
    LOG_INFO(">> PXQ (null)");
    return 0;
  }
  if (!Strings.hasPrefix(URI))
    return Ok(std::nullopt);
  
  CompactID PfxID = 0;
  const u64 NBits = Strings.getPrefixLogQ(URI);

  if (NBits) {
    LOG_POSITION(this);
    LOG_EXTRA("PXQ Decoding <{}>", NBits);
    exi_try_r(reader<StrmT>().readBits64(PfxID, NBits));
  }

#if EXI_LOGGING
  StrRef Pfx = Strings.getPrefix(URI, PfxID);
  LOG_INFO(">> PXQ @{}: \"{}\"", PfxID, Pfx);
#endif

  return Ok(PfxID);
}

template <typename StrmT>
ExiResult<CompactID> ExiDecoder::decodePfx(CompactID URI) {
  exi_invariant(Preserve.Prefixes, "NS event occurred without prefixes.");
  CompactID PfxID = 0;
  const u64 NBits = Strings.getPrefixLog(URI);

  if (NBits)
    LOG_POSITION(this);
  LOG_EXTRA("PXNS Decoding <{}>", NBits);
  exi_try_r(reader<StrmT>().readBits64(PfxID, NBits));

  StrRef Pfx;
  if (PfxID != 0) {
    // Cache hit
    PfxID -= 1;
    if EXI_UNLIKELY(!Strings.hasPrefix(URI, PfxID))
      return Err(ErrorCode::kInvalidEXIInput);
#if EXI_LOGGING
    Pfx = Strings.getPrefix(URI, PfxID);
    LOG_INFO(">> PXNS (Hit) @{}: \"{}\"", PfxID, Pfx);
#endif
  } else {
    // Cache miss
    SmallStr<32> Data;
    LOG_POSITION(this);
    StrRef Str = EXI_UNWRAP(reader<StrmT>().decodeString(Data));
    std::tie(Pfx, PfxID) = Strings.addPrefix(URI, Str);
    LOG_INFO(">> PXNS (Miss) @{}: \"{}\"", PfxID, Pfx);
  }

  return Ok(PfxID);
}

template <typename StrmT>
ExiResult<EventUID> ExiDecoder::decodeValue(SmallQName Name) {
  exi_invariant(Name.isQName());
  CompactID ValID; {
    LOG_POSITION(this);
    LOG_EXTRA("Decoding UInt");
    exi_try_r(reader<StrmT>().readUInt(ValID));
    LOG_EXTRA(">>> UInt {}", ValID);
  }

  if (ValID == 0) {
    // LocalValue hit
    const u64 NBits = Strings.getLocalValueLog(Name);
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(reader<StrmT>().readBits64(ValID, NBits));

#if EXI_LOGGING
    auto [URI, LocalName] = Strings.getQName(Name);
    StrRef LocalVal = Strings.getLocalValue(Name, ValID);
    LOG_INFO(">> LV (hit) @[{}:{}]:{}: \"{}\"",
      URI, LocalName, ValID, LocalVal);
#endif
    // Create unbound LocalValue.
    return EventUID::NewLocalValue(Name, ValID);
  } else if (ValID == 1) {
    // GlobalValue hit
    const u64 NBits = Strings.getGlobalValueLog();
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(reader<StrmT>().readBits64(ValID, NBits));

#if EXI_LOGGING
    StrRef GlobalVal = Strings.getGlobalValue(ValID);
    LOG_INFO(">> GV (hit) @{}: \"{}\"", ValID, GlobalVal);
#endif
    // Create unbound GlobalValue.
    return EventUID::NewGlobalValue(ValID);
  } else {
    // Cache miss
    const u64 Size = (ValID - 2);
    SmallStr<80> Data;
    StrRef Str = EXI_UNWRAP(readString<StrmT>(Size, Data));
    auto [Value, GID, LnID] = Strings.addValue(Name, Str);

#if EXI_LOGGING
    auto [URI, LocalName] = Strings.getQName(Name);
    LOG_INFO(">> LV (miss) @[{}:{}]:{}: \"{}\"",
      URI, LocalName, LnID, Value);
#endif
    // Newly created values are always returned as locals.
    // This makes implementations a bit simpler.
    return EventUID::NewLocalValue(Name, LnID);
  }
}

ExiResult<String> ExiDecoder::decodeString() {
  SmallStr<80> Data;
  if (auto E = Reader->decodeString(Data)
      .error_or(ExiError::OK)) [[unlikely]] {
    return Err(E);
  }
  return String(Data);
}

#include <exi/Decode/D/Decode.mac>
#define DECLARE_FUNCS(TYPE) EXI_DECODER_FUNCS_IMPL(, TYPE)
EXI_INSTANTIATE_DECODER_FUNCS(DECLARE_FUNCS)
