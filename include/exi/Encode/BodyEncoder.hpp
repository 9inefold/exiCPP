//===- exi/Encode/BodyEncoder.hpp ------------------------------------===//
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

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/Option.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Grammar/EncoderSchema.hpp>

namespace exi {

struct EncoderFlags {
  /// If the header was validated.
  bool ValidHeader : 1 = false;
  /// If the header has already been "written".
  bool DidHeader : 1 = false;
  /// If init has already been run.
  bool DidInit : 1 = false;
};

/// The top-level interface for encoder implementations.
class BodyEncoder {
  virtual void anchor();
public:
  virtual ~BodyEncoder() = default;
  virtual ExiError run() = 0;
};

/// The EXI encoding processor.
/// FIXME: Split this up into more implementations.
class ExiEncoder {
  /// The provided Header.
  ExiHeader Header = {};
  /// The schema for the current encoder.
  Box<encode::Schema> CurrentSchema;
  /// The encoder, the type of which is determined by the header.
  Box<BodyEncoder> TheEncoder;

  /// The stream used for diagnostics.
  Option<raw_ostream&> OS;
  /// State of the decoder in terms of progression.
  EncoderFlags Flags = {};

public:
  ExiEncoder(Option<raw_ostream&> OS = std::nullopt) : OS(OS) {}
  ExiEncoder(MaybeBox<ExiOptions> Opts, Option<raw_ostream&> OS = std::nullopt);
  ~ExiEncoder();

  /// Get the state flags.
  EncoderFlags flags() const { return Flags; }
  /// Returns if the header was successfully decoded.
  bool didHeader() const { return Flags.DidHeader && Flags.ValidHeader; }

  /// Returns the stream used for diagnostics.
  raw_ostream& os() const;

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Sets options for encoding.
  ExiError setOptions(MaybeBox<ExiOptions> Opts);
};

} // namespace exi
