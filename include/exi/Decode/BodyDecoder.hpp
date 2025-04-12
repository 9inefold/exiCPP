//===- exi/Basic/BodyDecoder.hpp ------------------------------------===//
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

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/DenseMap.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/StringTables.hpp>
#include <exi/Decode/HeaderDecoder.hpp>
#include <exi/Decode/UnifyBuffer.hpp>
#include <exi/Grammar/Schema.hpp>
#include <exi/Stream/StreamVariant.hpp>

namespace exi {

struct DecoderFlags {
  bool DidHeader : 1 = false;
};

/// The EXI decoding processor.
class ExiDecoder {
  ExiHeader Header;
  Poly<BitStreamReader, ByteStreamReader> Reader;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  decode::StringTable Idents;
  /// The schema for the current document.
  /// TODO: Add SchemaResolver...
  Box<Schema> CurrentSchema;
  /// The stream used for diagnostics.
  Option<raw_ostream&> OS;
  /// State of the decoder in terms of progression.
  DecoderFlags Flags;

public:
  ExiDecoder(Option<raw_ostream&> OS = std::nullopt) : OS(OS) {}
  ExiDecoder(MaybeBox<ExiOptions> Opts, Option<raw_ostream&> OS = std::nullopt);

  /*
  ExiDecoder(UnifiedBuffer Buffer,
             Option<raw_ostream&> OS = std::nullopt) : ExiDecoder(OS) {
    if (ExiError E = decodeHeader(Buffer); E && !OS)
      this->diagnose(E, true);
  }
  */

  /// Get the state flags.
  DecoderFlags flags() const { return Flags; }
  /// Returns if the header was successfully decoded.
  bool didHeader() const { return Flags.DidHeader; }

  /// Returns the stream used for diagnostics.
  raw_ostream& os() const EXI_READONLY; // TODO: Remove readonly?

  /// Diagnoses errors in the current context.
  void diagnose(ExiError E, bool Force = false) const;
  /// Diagnoses errors in the current context, then returns.
  ExiError diagnoseme(ExiError E) const {
    this->diagnose(E);
    return E;
  }

  /// Decodes the header from the provided buffer.
  /// Defined in `HeaderDecoder.cpp`.
  ExiError decodeHeader(UnifiedBuffer Buffer);

protected:

};

} // namespace exi
