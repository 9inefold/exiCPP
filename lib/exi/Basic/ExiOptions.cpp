//===- exi/Basic/ExiOptions.cpp -------------------------------------===//
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
/// This file defines the options in the EXI header.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/ExiOptions.hpp>
#include <core/Common/Option.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ErrorCodes.hpp>

#define DEBUG_TYPE "ExiOptions"

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

static ExiError ValidateCommon(const ExiOptions& Opts) {
  Option<bool> HasSchema = std::nullopt;
  auto CheckSchema = [&Opts, &HasSchema] () -> bool {
    if (HasSchema.has_value())
      return *HasSchema;
    // TODO: Add specific errors?
    HasSchema.emplace(HasValidSchemaID(Opts));
    return *HasSchema;
  };

  // Validate stream type.
  if (Opts.Compression && Opts.Alignment != AlignKind::PreCompression) {
    LOG_ERROR("invalid alignment for compression, must be chunked");
    return ExiError::HeaderAlign(Opts.Alignment, true);
  }

  // Validate chunking settings.
  if (Opts.SelfContained && Opts.Alignment == AlignKind::PreCompression) {
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

ExiError exi::ValidateOptions(const ExiOptions& Opts) {
  // Validate stream type.
  if (Opts.Compression) {
    using enum AlignKind;
    if (Opts.Alignment == BitPacked) {
      LOG_ERROR("bit alignment cannot be used with compression");
      return ExiError::HeaderAlign(BitPacked, true);
    } else if (Opts.Alignment == BytePacked) {
      LOG_ERROR("byte alignment cannot be used with precompression");
      return ExiError::HeaderAlign(BytePacked, true);
    }
  }

  return ValidateCommon(Opts);
}

ExiError exi::FixupAndValidateOptions(ExiOptions& Opts) {
  if (Opts.Alignment == AlignKind::None) {
    if (!Opts.Compression) {
      LOG_WARN("alignment not set, defaulting to bit packed");
      Opts.Alignment = AlignKind::BitPacked;
    } else /*Compression*/ {
      LOG_WARN("alignment not set, defaulting to pre-compression "
               "(compression is enabled)");
      Opts.Alignment = AlignKind::PreCompression;
    }
  }

  if (Opts.Compression) {
    using enum AlignKind;
    if (Opts.Alignment == BitPacked) {
      LOG_ERROR("bit alignment cannot be used with compression");
      // TODO: Permit updating this?
      return ExiError::HeaderAlign(BitPacked, true);
    } else if (Opts.Alignment != PreCompression) {
      LOG_WARN("alignment changed to precompression");
    }
    // Ensure correct setting is used.
    Opts.Alignment = PreCompression;
  }

  return ValidateCommon(Opts);
}
