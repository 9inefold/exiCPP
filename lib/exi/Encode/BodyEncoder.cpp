//===- exi/Encode/BodyEncoder.cpp -----------------------------------===//
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
/// This file provides the interface for encoding of the EXI body to a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/BodyEncoder.hpp>
#include <core/Common/MMatch.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/Except.hpp>
//#include <exi/Encode/ChannelEncoder.hpp>
#include <Encode/OrderedEncoder-inl.hpp>
#include <exi/Encode/Serializer.hpp>

#define DEBUG_TYPE "ExiEncoder"

using namespace exi;
using namespace exi::encode;

static ExiError ValidateBoxedOptions(MaybeBox<ExiOptions>& Opts) {
  if (!Opts) {
    LOG_ERROR("Passed null options.");
    return ExiError::kNullptrRef;
  }
  if (Opts.owned())
    return FixupAndValidateOptions(*Opts);
  else
    return ValidateOptions(*Opts);
}

//===----------------------------------------------------------------===//
// ExiEncoder
//===----------------------------------------------------------------===//

ExiEncoder::ExiEncoder(MaybeBox<ExiOptions>&& Opts, ExiError* Err) {
  ExiError::AsOutParam EAO(Err);
  if (auto E = setOptions(std::move(Opts))) {
    if EXI_UNLIKELY(!Err)
      Throw<argument_error>("Invalid options configuration.");
    EAO = std::move(E);
  }
}

ExiEncoder::~ExiEncoder() = default;

//////////////////////////////////////////////////////////////////////////
// Initialization

ExiResult<ExiEncoder> ExiEncoder::New(MaybeBox<ExiOptions>&& Opts) {
  if (auto E = ValidateBoxedOptions(Opts)) {
    LOG_ERROR("Invalid options configuration.");
    return Err(E);
  }
  return ExiEncoder(assumed_valid_tag{}, std::move(Opts));
}

ExiError ExiEncoder::setOptions(MaybeBox<ExiOptions>&& Opts) {
  if (Flags.DidHeader || Flags.DidInit) {
    LOG_ERROR("Header has already been written.");
    return ExiError::kInvalidConfig;
  } else if (auto E = ValidateBoxedOptions(Opts)) {
    LOG_ERROR("Invalid options configuration.");
    return E;
  }

  Header.Opts = std::move(Opts);
  Flags.ValidHeader = true;
  return ExiError::OK;
}

ExiError ExiEncoder::hdrHasCookie(bool HasCookie) {
  if (Flags.DidHeader || Flags.DidInit) {
    LOG_ERROR("Header has already been written.");
    return ExiError::kInvalidConfig;
  }

  Header.HasCookie = HasCookie;
  return ExiError::OK;
}

ExiError ExiEncoder::hdrHasOptions(bool IncludeOptions) {
  if (Flags.DidHeader || Flags.DidInit) {
    LOG_ERROR("Header has already been written.");
    return ExiError::kInvalidConfig;
  }

  Header.HasOptions = IncludeOptions;
  return ExiError::OK;
}

ExiError ExiEncoder::hdrVersion(u32 Version) {
  if (Flags.DidHeader || Flags.DidInit) {
    LOG_ERROR("Header has already been written.");
    return ExiError::kInvalidConfig;
  } else if EXI_NEVER(Version > kCurrentExiVersion) {
    LOG_ERROR("Invalid EXI version: {}.", Version);
    return ExiError::kInvalidConfig;
  }

  Header.ExiVersion = Version;
  return ExiError::OK;
}

ExiResult<ExiEncoder::EncoderFactory>
 ExiEncoder::setup(Option<bool> IncludeOptions) {
  if (!PCH) {
    // Try compiling the header.
    if (auto E = compileHeader(IncludeOptions))
      return Err(E);
  }
  // Set up schema.
  exi_try_r(this->init());
  return EncoderFactory(this);
}

ExiError ExiEncoder::init() {
  if (Flags.DidInit)
    return ExiError::OK;

  auto& Opts = *Header.Opts;
  if (!Opts.SchemaID.expect("schema is required"))
    ESFactory = BuiltinSchema::New(Opts);
  else
    exi_todo("real schemas are currently unsupported");
  
  if (!ESFactory) {
    LOG_ERROR("Schema factory could not be allocated.");
    return ErrorCode::kInvalidMemoryAlloc;
  }

  //if (hasDbgLogLevel(INFO))
  //  CurrentSchema->dump();

  LOG_EXTRA("Initialized!");
  return ExiError::OK;
}

template <class Encoder>
static ExiError ActuallyMakeEncoder(Box<Encoder>& BE,
                                    ExiOptions& Opts, encode::factory_t& F) {
  ExiError E = ExiError::OK;
  BE = std::make_unique<Encoder>(Opts, F, &E);
  // Basic validity checks.
  if EXI_NEVER(!BE)
    return ErrorCode::kInvalidMemoryAlloc;
  return E;
}

template <class Encoder>
static ExiResult<Box<BodyEncoder>>
 MakeEncoder(ExiOptions& Opts, encode::factory_t& F,
             auto& I, Option<BitBuffer> PCH) {
  if EXI_NEVER(!PCH) {
    LOG_ERROR("Invalid state, header never compiled!");
    return Err(ErrorCode::kInvalidConfig);
  }
  Box<Encoder> BE = nullptr;
  // Allocate and construct the encoder
  exi_try_r(ActuallyMakeEncoder<Encoder>(BE, Opts, F));
  // Initialize the encoder stream
  exi_try_r(BE->init(I));
  // Write the EXI header
  exi_try_r(BE->encodeHeader(*PCH, true));
  // All good!
  return BE;
}

/// UNSAFE! Only use options provided with `getOptions`.
static ExiOptions& UnwrapOptions(Option<const ExiOptions&> Opts) {
  return const_cast<ExiOptions&>(
    Opts.expect("Options should be initialized!"));
}

ALWAYS_INLINE ExiError ExiEncoder::EncoderFactory::encodeGeneric(Serializer* S, auto& I) {
  // TODO: Add method to skip checks after first initialization.
  auto& Opts = UnwrapOptions(This->getOptions());
  if (MMatch(Opts.Alignment).is(AlignKind::BitPacked,
                                AlignKind::BytePacked)) {
    ExiResult<Box<BodyEncoder>> BEOrErr
      = MakeEncoder<OrderedEncoder>(
        Opts, This->ESFactory, I, This->getPCH());
    if (BEOrErr.is_err())
      return BEOrErr.error();
    TheEncoder = std::move(*BEOrErr);
  } else {
    exi_todo("channel streams are unsupported.");
    return ExiError::TODO;
  }
  return ExiError::OK;
}

ExiError ExiEncoder::EncoderFactory::encode(Serializer* S, raw_ostream& Strm) {
  // FIXME: Should clear buffer first?
  exi_try(encodeGeneric(S, Strm));
  return this->go(S);
}

ExiError ExiEncoder::EncoderFactory::encode(Serializer* S,
                                            SmallVecImpl<char>& Buf) {
  // FIXME: Should clear buffer first?
  exi_try(encodeGeneric(S, Buf));
  return this->go(S);
}

ExiError ExiEncoder::EncoderFactory::go(Serializer* S) const {
  if EXI_UNLIKELY(!TheEncoder)
    return ErrorCode::kInvalidConfig;
  return S->run(&*TheEncoder);
}

#undef DEBUG_TYPE

//===----------------------------------------------------------------===//
// BodyEncoder
//===----------------------------------------------------------------===//

#define DEBUG_TYPE "BodyEncoder"

#if 0
ExiError BodyEncoder::SD() {
  LOG_EXTRA("Beginning encoding...");
  return ExiError::OK;
}

ExiError BodyEncoder::ED() {
  LOG_EXTRA("Completed encoding!");
  return ExiError::DONE;
}

ExiError BodyEncoder::CM(StrRef Comment) {
  LOG_EXTRA("Encoded CM");
  return ExiError::OK;
}

ExiError BodyEncoder::PI(StrRef Target, StrRef Text) {
  LOG_EXTRA("Encoded PI");
  return ExiError::OK;
}

ExiError BodyEncoder::DT(StrRef Name, StrRef PublicID,
                    StrRef SystemID, StrRef Text) {
  LOG_EXTRA("Encoded DT");
  return ExiError::OK;
}

ExiError BodyEncoder::ER(StrRef Name) {
  LOG_EXTRA("Encoded ER");
  return ExiError::OK;
}

ExiError BodyEncoder::SC() {
  LOG_ERROR("Cannot encode SC yet!");
  return ExiError::TODO;
}
#endif

void BodyEncoder::anchor() {}
