//===- exi/Encode/HeaderEncoder.cpp ---------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
/// This file implements encoding of the EXI Header to a stream.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/HeaderEncoder.hpp>
#include <core/Support/Casting.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/NBitInt.hpp>
//#include <exi/Decode/BodyEncoder.hpp>

#define DEBUG_TYPE "HeaderEncoder"

using namespace exi;

static bool HasValidSchemaID(const ExiOptions& Opts) {
  auto& Schema = Opts.SchemaID;
  if (!Schema.has_value())
    return false;
  const String* ID = Schema->data();
  if (ID == nullptr)
    return false;
  if (ID->empty())
    return false;
  // TODO: Validate schema?
  return true;
}

/// Checks if options are valid before attempting to write to stream.
// TODO: Move this to a more generic location? Could be used to validate in general.
static ExiError ValidateOptions(ExiOptions& Opts) {
  Option<bool> HasSchema = std::nullopt;
  auto CheckSchema = [&Opts, &HasSchema] () -> bool {
    if (HasSchema.has_value())
      return *HasSchema;
    // TODO: Add specific errors?
    HasSchema.emplace(HasValidSchemaID(Opts));
    return *HasSchema;
  };

  // Validate stream type.
  if (Opts.Alignment == AlignKind::None) {
    LOG_WARN("alignment not set, defaulting to bit packed");
    Opts.Alignment = AlignKind::BitPacked;
  }

  if (Opts.Compression && Opts.Alignment != AlignKind::PreCompression) {
    LOG_ERROR("invalid alignment for compression, must be chunked");
    return ExiError::HeaderAlign(Opts.Alignment, true);
  }

  // Validate chunking settings.
  if (Opts.SelfContained && Opts.Alignment != AlignKind::PreCompression) {
    LOG_ERROR("self-contained cannot be used with chunking");
    return ExiError::Mismatch();
  }

  // Validate strict mode.
  if (Opts.Strict) {
    auto PB = exi::make_preserve_builder(Opts.Preserve);
    if (PB.has(~PreserveKind::Strict)) {
      LOG_ERROR("invalid preserve options for strict mode");
      return ExiError::Mismatch();
    } else if (Opts.SelfContained) {
      LOG_ERROR("self-contained cannot be used in strict mode");
      return ExiError::Mismatch();
    }
  }

  // Validate datatype mapping.
  if (Opts.DatatypeRepresentationMap) {
    auto PB = exi::make_preserve_builder(Opts.Preserve);
    if (PB.has(PreserveKind::LexicalValues)) {
      LOG_ERROR("lexical value preservation cannot be used "
                "with datatype remapping");
      return ExiError::Mismatch();
    } else if (!CheckSchema()) {
      LOG_ERROR("datatype remapping cannot be done in schemaless mode");
      return ExiError::Mismatch();
    }
  }

  // Validate schema info.
  // TODO: Only when processing schema files.
  if (/*IsXMLSchema=*/false) {
    if (!CheckSchema()) {
      LOG_ERROR("blah blah schemas");
      return ExiError::TODO;
    }

    auto PB = exi::make_preserve_builder(Opts.Preserve);
    if (!PB.has(PreserveKind::Prefixes)) {
      LOG_ERROR("xml schemas must be encoded with prefixes preserved");
      return ExiError(ErrorCode::kNoPrefixesPreservedXMLSchema);
    }
  }

  // All good!
  return ExiError::OK;
}

/// Checks if header is valid before attempting to write to stream.
static ExiError DoPreliminaryOptionsCheck(const ExiHeader& Header) {
  // Ensure options are provided.
  // TODO: If permissive mode is added, allow deferred validation.
  if (!Header.Opts) {
    LOG_ERROR("options must be provided!");
    // FIXME: Incorrect error message for encoding
    return ExiError::Header();
  }

  // Validate version.
  if (Header.IsPreviewVersion)
    return ExiError::HeaderVer();
  else if (Header.ExiVersion > kCurrentExiVersion)
    return ExiError::HeaderVer(Header.ExiVersion);
  else if (Header.ExiVersion == 0)
    return ExiError::HeaderVer(0);
  
  // Validate stream type.
  auto& Opts = *Header.Opts;
  if (Opts.Compression) {
    using enum AlignKind;

    if (Opts.Alignment == BitPacked) {
      LOG_ERROR("bit alignment cannot be used with compression");
      return ExiError::HeaderAlign(BitPacked, true);
    } else if (Opts.Alignment != PreCompression) {
      LOG_WARN("alignment changed to precompression");
    }

    // Ensure correct setting is used.
    Opts.Alignment = PreCompression;
  }

  return ValidateOptions(Opts);
}

static ExiError EncodeVersion(const ExiHeader& Header, BitWriter* Strm) {
  // Should always be false.
  exi_invariant(Header.IsPreviewVersion == false);
  Strm->writeBit(/*PreviewVersion=*/false);

  static constexpr u32 VersionChunk = 0b1111;
  u32 Version = Header.ExiVersion - 1;

  // This assume has been tested on GCC and Clang.
  // It WILL remove/unroll the loop, as seen here https://godbolt.org/z/h6hfcPPqh.
  exi_assume(Version < kCurrentExiVersion);
  for (; Version >= VersionChunk; Version -= VersionChunk) {
    Strm->writeBits<4>(VersionChunk);
    Version -= VersionChunk;
  }

  Strm->writeBits<4>(Version);
  return ExiError::OK;
}

static ExiError encodeHeaderImpl(const ExiHeader& Header, BitWriter& Strm) {
  if (ExiError E = DoPreliminaryOptionsCheck(Header)) {
    // There was some error with encoding settings.
    LOG_ERROR("error with header settings");
    return E;
  }

  if (Header.HasCookie) {
    Strm.writeString("$EXI");
    LOG_EXTRA("header has cookie.");
  }

  Strm.writeBits<2>(0b10);
  Strm.writeBit(Header.HasOptions);
  
  exi_try(EncodeVersion(Header, &Strm));
  LOG_EXTRA("EXI version: {}", Header.ExiVersion);

  if (!Header.HasOptions)
    LOG_EXTRA("options are out-of-band");
  else {
    // TODO: Encode options to file.
    exi_unreachable("options encode unimplemented");
    return ExiError::TODO;
  }

  if (Header.Opts->Alignment != AlignKind::BitPacked)
    // Skip [Padding Bits].
    Strm.align();

  return ExiError::OK;
}

ExiError exi::encodeHeader(const ExiHeader& Header, OrdWriter& Strm) {
  if EXI_UNLIKELY(Strm.empty()) {
    LOG_ERROR("empty stream cannot be used!");
    return ExiError(ErrorCode::kUnexpectedError);
  }

  if (auto* Bits = dyn_cast<BitWriter>(&Strm)) {
    // Header must be bit packed. If the representation is the same, do nothing.
    return encodeHeaderImpl(Header, *Bits);
  }

  auto& Bytes = cast<ByteWriter>(Strm);
  BitWriter Bits(Bytes.getProxy());

  ExiError Out = encodeHeaderImpl(Header, Bits);
  Bytes.setProxy(Bits.getProxy());
  return Out;
}
