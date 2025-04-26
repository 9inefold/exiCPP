//===- exi/Basic/BodyDecoder.cpp ------------------------------------===//
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
#include <fmt/ranges.h>

#define DEBUG_TYPE "BodyDecoder"

#if 0
# define LOG_POSITION(...)                                                    \
  LOG_EXTRA("@[{}]:", ((__VA_ARGS__)->Reader->bitPos()))
# define LOG_META(...) LOG_EXTRA(__VA_ARGS__)
#else
# define LOG_POSITION(...) ((void)(0))
# define LOG_META(...) ((void)(0))
#endif

using namespace exi;

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
    Reader.emplace<bitstream::BitReader>(Buffer.arr());
  else
    Reader.emplace<bitstream::ByteReader>(Buffer.arr());

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

ExiError ExiDecoder::decodeBody() {
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
  LOG_EXTRA("Beginning decoding...");

  auto* SchemaPtr = CurrentSchema.get();
  if (auto* CS = dyn_cast<CompiledSchema>(SchemaPtr)) {
    // TODO: Schemas!!!
    exi_unreachable("compiled schemas currently unsupported.");
  }

  exi_assert(isa<BuiltinSchema>(*SchemaPtr));
  while (!Reader->isFull()) {
    ExiError E = this->decodeEvent();
    if (E == ExiError::OK)
      continue;
    else if (E == ErrorCode::kParsingComplete)
      break;
    // Some other error code.
    return E;
  }

  LOG_EXTRA("Completed decoding!");
  return ExiError::OK;
}

ExiError ExiDecoder::decodeEvent() {
  LOG_POSITION(this);
  const EventUID Event = CurrentSchema->decode(this);
  
  switch (Event.getTerm()) {
  case EventTerm::SD:       // Start Document
    return ExiError::OK;
  case EventTerm::ED:       // End Document
    return ErrorCode::kParsingComplete;
  case EventTerm::SE:       // Start Element (*)
  case EventTerm::SEUri:    // Start Element (uri:*)
  case EventTerm::SEQName:  // Start Element (qname)
    return this->handleSE(Event);
  case EventTerm::EE:       // End Element
    return this->handleEE(Event);
  case EventTerm::AT:       // Attribute (*, value)
  case EventTerm::ATUri:    // Attribute (uri:*, value)
  case EventTerm::ATQName:  // Attribute (qname, value)
    return this->handleAT(Event);
  case EventTerm::NS:       // Namespace Declaration (uri, prefix, local-element-ns)
    return this->handleNS(Event);
  case EventTerm::CH:       // Characters (value)
  case EventTerm::CHExtern: // Characters (external-value)
    return this->handleCH(Event);
  case EventTerm::CM:       // Comment text (text)  
  case EventTerm::PI:       // Processing Instruction (name, text)
  case EventTerm::DT:       // DOCTYPE (name, public, system, text)
  case EventTerm::ER:       // Entity Reference (name)
  case EventTerm::SC:       // Self Contained
    return ErrorCode::kUnimplemented;
  default:
    exi_assert("unknown term");
    return ErrorCode::kInvalidEXIInput;
  }
}

//////////////////////////////////////////////////////////////////////////
// Terms

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

  auto QName = SmallQName::MakeQName(URI, LNI);
  return Ok(EventUID::NewQName(QName, Pfx));
}

ExiResult<EventUID> ExiDecoder::decodeNS() {
  const CompactID URI = $unwrap(decodeURI());
  const CompactID PfxID = $unwrap(decodePfx(URI));

  bool IsLocal = false;
  exi_try_r(Reader->readBit(IsLocal));

  auto QName = SmallQName::MakeURI(URI);
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
    StrRef Str = $unwrap(decodeString(Data));
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
    StrRef Str = $unwrap(readString(LnID, Data));
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
    StrRef Str = $unwrap(decodeString(Data));
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

ExiResult<StrRef> ExiDecoder::decodeString(SmallVecImpl<char>& Storage) {
  u64 Size = 0;
  if (auto E = Reader->readUInt(Size))
    return Err(E);
  return readString(Size, Storage);
}

ExiResult<StrRef> ExiDecoder::readString(u64 Size, SmallVecImpl<char>& Storage) {
  LOG_POSITION(this);
  if (Size == 0)
    return ""_str;

  Storage.clear();
  Storage.reserve(Size);

  for (u64 Ix = 0; Ix < Size; ++Ix) {
    u64 Rune;
    if (auto E = Reader->readUInt(Rune)) [[unlikely]] {
      LOG_ERROR("Invalid Rune at [{}:{}].", Ix, Size);
      return Err(E);
    }
    auto Buf = RuneEncoder::Encode(Rune);
    Storage.append(Buf.data(), Buf.data() + Buf.size());
    LOG_EXTRA(">>> {}: {}", Buf.str(), 
      fmt::format("0x{:02X}", fmt::join(Buf, " 0x")));
  }

  return StrRef(Storage.data(), Size);
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
    os() << "At [" << Reader->bitPos() << "]: ";
  }
  os() << E << '\n';
}
