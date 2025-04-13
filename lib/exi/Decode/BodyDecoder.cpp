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
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>

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
    CurrentSchema = BuiltinSchema::GetSchema(Opts.SelfContained);
  else
    exi_unreachable("schemas are currently unsupported");
  
  if (!CurrentSchema) {
    LOG_ERROR("Schema could not be allocated.");
    return ErrorCode::kInvalidMemoryAlloc;
  }
  
  // TODO: Load schema
  Idents.setup(*Header.Opts);
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
    return this->handleEE();
  case EventTerm::AT:       // Attribute (*, value)
  case EventTerm::ATUri:    // Attribute (uri:*, value)
  case EventTerm::ATQName:  // Attribute (qname, value)
    return this->decodeAT(Term);
  case EventTerm::NS:       // Namespace Declaration (uri, prefix, local-element-ns)
    return this->handleNS();
  case EventTerm::CH:       // Characters (value)
    return this->handleCH();
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

ExiError ExiDecoder::decodeSE(EventTerm Term) {
  // Start Element (*)
  // Start Element (uri:*)
  // Start Element (qname)
  return ErrorCode::kUnimplemented;
}

ExiError ExiDecoder::decodeAT(EventTerm Term) {
  // Attribute (*, value)
  // Attribute (uri:*, value)
  // Attribute (qname, value)
  return ErrorCode::kUnimplemented;
}

ExiError ExiDecoder::handleEE() {
  return ErrorCode::kUnimplemented;
}

ExiError ExiDecoder::handleNS() {
  // Namespace Declaration (uri, prefix, local-element-ns)
  return ErrorCode::kUnimplemented;
}

ExiError ExiDecoder::handleCH() {
  // Characters (value)
  return ErrorCode::kUnimplemented;
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
