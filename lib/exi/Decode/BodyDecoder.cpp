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
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/Runes.hpp>

#define DEBUG_TYPE "BodyDecoder"

using namespace exi;

ExiDecoder::ExiDecoder(MaybeBox<ExiOptions> Opts,
                       Option<raw_ostream&> OS) : ExiDecoder(OS) {
  Header.Opts = std::move(Opts);
}

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

  CurrentSchema->dump();
  
  // TODO: Load schema
  Idents.setup(Opts);
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
  const EventTerm Term = CurrentSchema->getTerm(Reader);
  LOG_EXTRA("> {}: {}",
    get_event_name(Term),
    get_event_signature(Term)
  );
  
  switch (Term) {
  case EventTerm::SD:       // Start Document
    return ExiError::OK;
  case EventTerm::ED:       // End Document
    return ErrorCode::kParsingComplete;
  case EventTerm::SE:       // Start Element (*)
  case EventTerm::SEUri:    // Start Element (uri:*)
  case EventTerm::SEQName:  // Start Element (qname)
    return this->decodeSE(Term);
  case EventTerm::EE:       // End Element
    return this->decodeEE();
  case EventTerm::AT:       // Attribute (*, value)
  case EventTerm::ATUri:    // Attribute (uri:*, value)
  case EventTerm::ATQName:  // Attribute (qname, value)
    return this->decodeAT(Term);
  case EventTerm::NS:       // Namespace Declaration (uri, prefix, local-element-ns)
    return this->decodeNS();
  case EventTerm::CH:       // Characters (value)
    return this->decodeCH();
  case EventTerm::CM:       // Comment text (text)  
  case EventTerm::PI:       // Processing Instruction (name, text)
  case EventTerm::DT:       // DOCTYPE (name, public, system, text)
  case EventTerm::ER:       // Entity Reference (name)
  case EventTerm::SC:       // Self Contained
    return ErrorCode::kUnimplemented;
  default:
    exi_unreachable("unknown term");
  }
}

// Start Element (*)
// Start Element (uri:*)
// Start Element (qname)
ExiError ExiDecoder::decodeSE(EventTerm Term) {
  if (Term != EventTerm::SE)
    return ErrorCode::kUnimplemented;
  
  u64 UID; {
    const u64 NBits = Idents.getURILog();
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try(Reader->readBits64(UID, NBits));
  }

  StrRef URI;
  if (UID == 0) {
    SmallStr<32> Data;
    Option Str = decodeString(Data);
    if EXI_UNLIKELY(!Str)
      return ErrorCode::kInvalidStringOp;
    std::tie(URI, UID) = Idents.addURI(*Str);
  } else {
    UID -= 1;
    URI = Idents.getURI(UID);
  }
  LOG_EXTRA(">> URI @{}: \"{}\"", UID, URI);

  u64 LnID; {
    LOG_EXTRA("Decoding UInt");
    exi_try(Reader->readUInt(LnID));
  }

  StrRef LocalName;
  if (LnID == 0) {
    const u64 NBits = Idents.getLocalNameLog(UID);
    LOG_EXTRA("Decoding <{}>", NBits);
    exi_try(Reader->readBits64(LnID, NBits));
    LocalName = Idents.getLocalName(UID, LnID);
  } else {
    SmallStr<32> Data;
    Option Str = readString(LnID, Data);
    if EXI_UNLIKELY(!Str)
      return ErrorCode::kInvalidStringOp;
    LocalName = Idents.addLocalName(UID, *Str);
  }
  LOG_EXTRA(">> LN @{}: \"{}\"", LnID, LocalName);
  
  // TODO: Prefixes...

  return ExiError::OK;
}

ExiError ExiDecoder::decodeEE() {
  return ErrorCode::kUnimplemented;
}

// Attribute (*, value)
// Attribute (uri:*, value)
// Attribute (qname, value)
ExiError ExiDecoder::decodeAT(EventTerm Term) {
  if (Term != EventTerm::AT)
    return ErrorCode::kUnimplemented;
  
  return ErrorCode::kUnimplemented;
}

// Namespace Declaration (uri, prefix, local-element-ns)
ExiError ExiDecoder::decodeNS() {
  return ErrorCode::kUnimplemented;
}

// Characters (value)
ExiError ExiDecoder::decodeCH() {
  return ErrorCode::kUnimplemented;
}

Option<String> ExiDecoder::decodeString() {
  SmallStr<64> Data;
  if (!this->decodeString(Data))
    return std::nullopt;
  return String(Data);
}

Option<StrRef> ExiDecoder::decodeString(SmallVecImpl<char>& Storage) {
  u64 Size = 0;
  if (auto E = Reader->readUInt(Size)) {
    this->diagnose(E);
    return std::nullopt;
  }

  return readString(Size, Storage);
}

Option<StrRef> ExiDecoder::readString(u64 Size, SmallVecImpl<char>& Storage) {
  if (Size == 0)
    return ""_str;

  Storage.clear();
  for (u64 Ix = 0; Ix < Size; ++Ix) {
    u64 Rune;
    if (auto E = Reader->readUInt(Rune)) {
      this->diagnose(E);
      return std::nullopt;
    }
    auto Buf = RuneEncoder::Encode(Rune);
    Storage.append(Buf.data(), Buf.data() + Buf.size());
  }

  return StrRef(Storage.data(), Size);
}

//===----------------------------------------------------------------===//
// Miscellaneous
//===----------------------------------------------------------------===//

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
