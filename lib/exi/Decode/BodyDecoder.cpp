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
#include <core/Common/MMatch.hpp>
#include <core/Common/Unwrap.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/Runes.hpp>
#include <exi/Decode/Serializer.hpp>
#include <fmt/ranges.h>

#define DEBUG_TYPE "BodyDecoder"

#if 1
// FIXME: Update if needed...
# define LOG_POSITION(...)                                                    \
  LOG_EXTRA("@[{}]:", ((__VA_ARGS__)->Reader->bitPos()))
# define LOG_META(...) LOG_EXTRA(__VA_ARGS__)
#else
# define LOG_POSITION(...) ((void)(0))
# define LOG_META(...) ((void)(0))
#endif

using namespace exi;
using namespace exi::decode;

ExiDecoder::ExiDecoder(MaybeBox<ExiOptions> Opts,
                       Option<raw_ostream&> OS) : ExiDecoder(OS) {
  Header.Opts = std::move(Opts);
}

//////////////////////////////////////////////////////////////////////////
// Initialization

ExiError ExiDecoder::readerExists() const {
  if (!Reader.empty()) {
    LOG_ERROR("Invalid processor state!");
    return ErrorCode::kInvalidConfig;
  }
  return ExiError::OK;
}

ExiError ExiDecoder::setOptions(MaybeBox<ExiOptions> Opts) {
  if (Flags.DidHeader) {
    exi_invariant(Header.Opts, "Options not initialized!");
    return this->readerExists();
  }
  Header.Opts = std::move(Opts);
  if (!Reader.empty())
    Flags.DidHeader = true;
  
  LOG_EXTRA("Options set manually.");
  return ExiError::OK;
}

ExiError ExiDecoder::setReader(UnifiedBuffer Buffer) {
  if (!Header.Opts) {
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
    return this->readerExists();
  }

  if (!Header.Opts || Reader.empty()) {
    LOG_ERROR("Options or Reader are not initialized.");
    return ErrorCode::kInvalidConfig;
  }

  auto& Opts = *Header.Opts;
  if (!Opts.SchemaID.expect("schema is required"))
    CurrentSchema = BuiltinSchema::New(Opts);
  else
    exi_unreachable("schemas are currently unsupported");
  
  if (!CurrentSchema) {
    LOG_ERROR("Schema could not be allocated.");
    return ErrorCode::kInvalidMemoryAlloc;
  }

  if (hasDbgLogLevel(INFO))
    CurrentSchema->dump();
  // TODO: Load schema
  Idents.setup(Opts);

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
    exi_unreachable("dynamic and compiled schemas currently unsupported.");
  }

  return ExiError::OK;
}

ExiError ExiDecoder::decodeBody() {
  Serializer S{};
  return this->decodeBody(&S);
}

ExiError ExiDecoder::decodeBody(Serializer* S) {
  if (S == nullptr) {
    // TODO: Allow defaulting in permissive mode?
    LOG_ERROR("Serializer cannot be null!");
    return ErrorCode::kInvalidEXIInput;
  }

  if (ExiError E = prepareForDecoding())
    return E;

  //LOG_EXTRA("Beginning decoding...");
  while (Reader->hasData()) {
    ExiError E = this->decodeEvent(S);
    if EXI_LIKELY(E == ExiError::OK)
      continue;
    else if (E == ExiError::DONE)
      break;
    // Some other error code.
    return E;
  }

  //LOG_EXTRA("Completed decoding!");
  return ExiError::OK;
}

EXI_HOT ExiError ExiDecoder::decodeEvent(Serializer* S) {
  LOG_POSITION(this);
  const EventUID Event = CurrentSchema->decode(this);
  
  switch (Event.getTerm()) {
  case EventTerm::SE:       // Start Element (*)
  case EventTerm::SEUri:    // Start Element (uri:*)
  case EventTerm::SEQName:  // Start Element (qname)
    return this->handleSE(S, Event);
  case EventTerm::EE:       // End Element
    return this->handleEE(S, Event);
  case EventTerm::AT:       // Attribute (*, value)
  case EventTerm::ATUri:    // Attribute (uri:*, value)
  case EventTerm::ATQName:  // Attribute (qname, value)
    return this->handleAT(S, Event);
  case EventTerm::NS:       // Namespace Declaration (uri, prefix, local-element-ns)
    return this->handleNS(S, Event);
  case EventTerm::CH:       // Characters (value)
  case EventTerm::CHExtern: // Characters (external-value)
    return this->handleCH(S, Event);
  default:
    return this->dispatchUncommonEvent(S, Event);
  }
}

EXI_COLD ExiError ExiDecoder::dispatchUncommonEvent(Serializer* S,
                                                    const EventUID Event) {
  // ...
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

#if 0
// Start Element (*)
// Start Element (uri:*)
// Start Element (qname)
ExiError ExiDecoder::handleSE(EventUID Event) {
  LOG_EXTRA("Decoded SE");
  return ExiError::OK;
}

ExiError ExiDecoder::handleEE(EventUID Event) {
#if EXI_LOGGING
  if (Event.hasQName()) {
    StrRef URI = this->getPfxOrURI(Event);
    StrRef LocalName = Idents.getLocalName(Event.Name);
    LOG_INFO(">> EE[{}:{}]\n", URI, LocalName);
  } else {
    LOG_EXTRA("Decoded EE");
    if (hasDbgLogLevel(INFO))
      dbgs() << '\n';
  }
#endif
  return ExiError::OK;
}

// Attribute (*, value)
// Attribute (uri:*, value)
// Attribute (qname, value)
ExiError ExiDecoder::handleAT(EventUID Event) {
  exi_invariant(Event.hasQName());
  Result R = decodeValue(Event.Name);
  const auto Value = $unwrap(std::move(R));
  LOG_EXTRA("Decoded AT");
  return ExiError::OK;
}

// Namespace Declaration (uri, prefix, local-element-ns)
ExiError ExiDecoder::handleNS(EventUID) {
  const auto Event = $unwrap(decodeNS());
  LOG_EXTRA("Decoded NS");
  return ExiError::OK;
}

// Characters (value)
ExiError ExiDecoder::handleCH(EventUID Event) {
  LOG_EXTRA("Decoded CH");
  return ExiError::OK;
}
#endif

// Start Element (*)
// Start Element (uri:*)
// Start Element (qname)
ExiError ExiDecoder::handleSE(Serializer* S, EventUID Event) {
  const QName Name = this->getQName(Event);
  LOG_EXTRA("Decoded SE");
  return S->SE(Name);
}

ExiError ExiDecoder::handleEE(Serializer* S, EventUID Event) {
  if (!Event.hasQName()) {
    LOG_EXTRA("Decoded EE");
    if (hasDbgLogLevel(INFO))
      dbgs() << '\n';
    return ExiError::OK;
  }

  const QName Name = this->getQName(Event);
  LOG_INFO(">> EE[{}:{}]\n", Name.Prefix, Name.Name);
  return S->EE(Name);
}

// Attribute (*, value)
// Attribute (uri:*, value)
// Attribute (qname, value)
ExiError ExiDecoder::handleAT(Serializer* S, EventUID Event) {
  exi_invariant(Event.hasQName());
  Result R = decodeValue(Event.Name);
  const auto ValueID = $unwrap(std::move(R));

  const QName Name = this->getQName(Event);
  StrRef Value = Idents.getValue(ValueID);

  LOG_EXTRA("Decoded AT");
  return S->AT(Name, Value);
}

// Namespace Declaration (uri, prefix, local-element-ns)
ExiError ExiDecoder::handleNS(Serializer* S, EventUID) {
  const auto Event = $unwrap(decodeNS());
  const auto Name = Event.Name;
  
  StrRef URI = Idents.getURI(Name.URI);
  StrRef Pfx = Idents.getPrefix(Name.URI, Event.Prefix);

  LOG_EXTRA("Decoded NS");
  return S->NS(URI, Pfx, Event.isLocal());
}

// Characters (value)
ExiError ExiDecoder::handleCH(Serializer* S, EventUID Event) {
  StrRef Value = Idents.getValue(Event);
  LOG_EXTRA("Decoded CH");
  return S->CH(Value);
}

#define READ_STRING(NAME, RESERVE, READER)                                    \
  SmallStr<RESERVE> NAME##_Data;                                              \
  Result NAME = (READER)->decodeString(NAME##_Data);                          \
  if EXI_UNLIKELY(NAME.is_err())                                              \
    return NAME.error();

ExiError ExiDecoder::handleCM(Serializer* S) {
  READ_STRING(Comment, 80, Reader)
  if (S->needsPersistence())
    this->internStrings(*Comment);
  return S->CM(*Comment);
}

ExiError ExiDecoder::handlePI(Serializer* S) {
  return Reader.visit([this, S] (auto& Strm) -> ExiError {
    READ_STRING(Target, 16, &Strm)
    READ_STRING(Text,   48, &Strm)
    if (S->needsPersistence())
      this->internStrings(*Target, *Text);
    return S->PI(*Target, *Text);
  });
}

ExiError ExiDecoder::handleDT(Serializer* S) {
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

ExiError ExiDecoder::handleER(Serializer* S) {
  READ_STRING(Entity, 16, Reader)
  if (S->needsPersistence())
    this->internStrings(*Entity);
  return S->CM(*Entity);
}

//////////////////////////////////////////////////////////////////////////
// Util

QName ExiDecoder::getQName(EventUID Event) {
  exi_invariant(Event.hasQName());
  auto [URI, LocalName] = Idents.getQName(Event.Name);
  if (!Event.hasPrefix()) {
    return QName {
      .URI = URI,
      .Name = LocalName
    };
  }

  StrRef Pfx = Idents.getPrefix(
    Event.getURI(), Event.getPrefix()
  );
  return QName {
    .URI = URI,
    .Name = LocalName,
    .Prefix = Pfx
  };
}

StrRef ExiDecoder::getPfxOrURI(EventUID Event) {
  if (!Event.hasQName())
    return "*"_str;
  
  const CompactID URI = Event.getURI();
  if (!Idents.hasPrefix(URI)) {
    LOG_META("No Prefix for @{}: {}", URI, Preserve.Prefixes);
    if (Idents.hasURI(URI))
      return Idents.getURI(URI);
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

  return Idents.getURI(URI);
}

Option<StrRef> ExiDecoder::tryGetPfx(CompactID URI, CompactID PfxID) {
  if (!Idents.hasPrefix(URI, PfxID))
    return std::nullopt;
  
  StrRef Pfx = Idents.getPrefix(URI, PfxID);
  if (Pfx.empty() && URI != 0) {
    // Don't allow arbitrary empty prefixes when printing.
    // May be confusing for the reader.
    return std::nullopt;
  }

  return Pfx;
}

//////////////////////////////////////////////////////////////////////////
// Values

ExiResult<EventUID> ExiDecoder::decodeQName() {
  const CompactID URI = $unwrap(decodeURI());
  const CompactID LNI = $unwrap(decodeName(URI));
  Option Pfx = $unwrap(decodePfxQ(URI));

  auto QName = SmallQName::NewQName(URI, LNI);
  return Ok(EventUID::NewQName(QName, Pfx));
}

ExiResult<EventUID> ExiDecoder::decodeNS() {
  const CompactID URI = $unwrap(decodeURI());
  const CompactID PfxID = $unwrap(decodePfx(URI));

  bool IsLocal = false;
  exi_try_r(Reader->readBit(IsLocal));
  if (!IsLocal) {
    LOG_INFO(">> NONLOCAL");
    return Err(ErrorCode::kUnimplemented);
  }

  auto QName = SmallQName::NewURI(URI);
  return Ok(EventUID::NewNS(QName, PfxID, IsLocal));
}

ExiResult<CompactID> ExiDecoder::decodeURI() {
  CompactID URI; {
    const u64 NBits = Idents.getURILog();
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(Reader->readBits64(URI, NBits));
  }

  if (URI == 0) {
    // Cache miss
    StrRef URIStr;
    SmallStr<32> Data;
    LOG_POSITION(this);
    StrRef Str = $unwrap(Reader->decodeString(Data));
    std::tie(URIStr, URI) = Idents.addURI(Str);
    LOG_INFO(">> URI(Miss) @{}: \"{}\"", URI, URIStr);
  } else {
    // Cache hit
    URI -= 1;
#if EXI_LOGGING
    StrRef URIStr = Idents.getURI(URI);
    LOG_INFO(">> URI(Hit) @{}: \"{}\"", URI, URIStr);
#endif
  }

  return URI;
}

ExiResult<CompactID> ExiDecoder::decodeName(CompactID URI) {
  CompactID LnID; {
    LOG_POSITION(this);
    LOG_EXTRA("Decoding UInt");
    exi_try_r(Reader->readUInt(LnID));
    LOG_EXTRA(">>> UInt {}", LnID);
  }

  StrRef LocalName;
  if (LnID == 0) {
    // Cache hit
    const u64 NBits = Idents.getLocalNameLog(URI);
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(Reader->readBits64(LnID, NBits));
#if EXI_LOGGING
    LocalName = Idents.getLocalName(URI, LnID);
#endif
  } else {
    // Cache miss
    LnID -= 1;
    SmallStr<32> Data;
    StrRef Str = $unwrap(Reader->readString(LnID, Data));
    std::tie(LocalName, LnID) = Idents.addLocalName(URI, Str);
  }

  LOG_INFO(">> LN @{}: \"{}\"", LnID, LocalName);
  return LnID;
}

ExiResult<Option<CompactID>> ExiDecoder::decodePfxQ(CompactID URI) {
  if (!Preserve.Prefixes)
    return Ok(std::nullopt);
  if (!Idents.hasPrefix(URI))
    return Ok(std::nullopt);
  
  CompactID PfxID = 0;
  const u64 NBits = Idents.getPrefixLogQ(URI);

  if (NBits) {
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(Reader->readBits64(PfxID, NBits));
  }

#if EXI_LOGGING
  StrRef Pfx = Idents.getPrefix(URI, PfxID);
  LOG_INFO(">> PXQ @{}: \"{}\"", PfxID, Pfx);
#endif

  return Ok(PfxID);
}

ExiResult<CompactID> ExiDecoder::decodePfx(CompactID URI) {
  exi_invariant(Preserve.Prefixes, "NS event occurred without prefixes.");
  CompactID PfxID = 0;
  const u64 NBits = Idents.getPrefixLog(URI);

  LOG_POSITION(this);
  LOG_EXTRA("Decoding <{}>", NBits);
  exi_try_r(Reader->readBits64(PfxID, NBits));

  StrRef Pfx;
  if (PfxID != 0) {
    // Cache hit
    PfxID -= 1;
    if EXI_UNLIKELY(!Idents.hasPrefix(URI, PfxID))
      return Err(ErrorCode::kInvalidEXIInput);
#if EXI_LOGGING
    Pfx = Idents.getPrefix(URI, PfxID);
#endif
  } else {
    // Cache miss
    SmallStr<32> Data;
    StrRef Str = $unwrap(Reader->decodeString(Data));
    std::tie(Pfx, PfxID) = Idents.addPrefix(URI, Str);
  }

  LOG_INFO(">> PXNS @{}: \"{}\"", PfxID, Pfx);
  return Ok(PfxID);
}

ExiResult<EventUID> ExiDecoder::decodeValue(SmallQName Name) {
  exi_invariant(Name.isQName());
  CompactID ValID; {
    LOG_POSITION(this);
    LOG_EXTRA("Decoding UInt");
    exi_try_r(Reader->readUInt(ValID));
    LOG_EXTRA(">>> UInt {}", ValID);
  }

  if (ValID == 0) {
    // LocalValue hit
    const u64 NBits = Idents.getLocalValueLog(Name);
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(Reader->readBits64(ValID, NBits));

#if EXI_LOGGING
    auto [URI, LocalName] = Idents.getQName(Name);
    StrRef LocalVal = Idents.getLocalValue(Name, ValID);
    LOG_INFO(">> LV @[{}:{}]:{}: \"{}\"",
      URI, LocalName, ValID, LocalVal);
#endif
    // Create unbound LocalValue.
    return EventUID::NewLocalValue(Name, ValID);
  } else if (ValID == 1) {
    // GlobalValue hit
    const u64 NBits = Idents.getGlobalValueLog();
    LOG_POSITION(this);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try_r(Reader->readBits64(ValID, NBits));

#if EXI_LOGGING
    StrRef GlobalVal = Idents.getGlobalValue(ValID);
    LOG_INFO(">> GV @{}: \"{}\"", ValID, GlobalVal);
#endif
    // Create unbound GlobalValue.
    return EventUID::NewGlobalValue(ValID);
  } else {
    // Cache miss
    const u64 Size = (ValID - 2);
    SmallStr<32> Data;
    StrRef Str = $unwrap(readString(Size, Data));
    auto [Value, GID, LnID] = Idents.addValue(Name, Str);

#if EXI_LOGGING
    auto [URI, LocalName] = Idents.getQName(Name);
    LOG_INFO(">> LV @[{}:{}]:{}: \"{}\"",
      URI, LocalName, LnID, Value);
#endif
    // Newly created values are always returned as locals.
    // This makes implementations a bit simpler.
    return EventUID::NewLocalValue(Name, LnID);
  }
}

ExiResult<String> ExiDecoder::decodeString() {
  SmallStr<64> Data;
  if (auto E = this->decodeString(Data)
      .error_or(ExiError::OK)) [[unlikely]] {
    return Err(E);
  }
  return String(Data);
}

//////////////////////////////////////////////////////////////////////////
// Miscellaneous

raw_ostream& ExiDecoder::os() const {
  return OS.value_or(errs());
}

void ExiDecoder::diagnose(ExiError E, bool Force) const {
  if (E == ExiError::OK)
    return;
  if (!Force && !OS)
    return;
  
  if EXI_LIKELY(!Reader.empty()) {
    // os() << "At [" << Reader->bitPos() << "]: ";
  }
  os() << E << '\n';
}
