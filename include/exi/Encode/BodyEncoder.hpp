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

/// The top-level interface for encoder implementations.
class BodyEncoder {
protected:
  /// The options for the current encoding
  const ExiOptions& Opts;
public:
  BodyEncoder(ExiOptions& Opts);
  virtual ~BodyEncoder() = default;

  /// Start Document
  virtual ExiError SD();
  /// End Document
  virtual ExiError ED();

  /// Start Element (with prefix)
  virtual ExiError SE_Pfx(StrRef Name, StrRef Pfx) {
    return ExiError::OK;
  }
  /// Start Element (with URI) - for local-element-ns
  virtual ExiError SE_Uri(StrRef Name, StrRef URI) {
    return ExiError::OK;
  }
  /// End Element
  virtual ExiError EE() {
    return ExiError::OK;
  }
  /// Attribute
  virtual ExiError AT(StrRef Name, StrRef Pfx, StrRef Value) {
    return ExiError::OK;
  }

  // TODO: Make typed AT variants that forward to normal AT by default.

  /// Namespace Declaration
  virtual ExiError NS(StrRef URI, StrRef Prefix) {
    return ExiError::OK;
  }
  /// Characters
  virtual ExiError CH(StrRef Value) {
    return ExiError::OK;
  }

  /// Comment
  virtual ExiError CM(StrRef Comment);
  /// Processing Instruction
  virtual ExiError PI(StrRef Target, StrRef Text);
  /// DOCTYPE
  virtual ExiError DT(StrRef Name, StrRef PublicID,
                      StrRef SystemID, StrRef Text);
  /// Entity Reference
  virtual ExiError ER(StrRef Name);
  
  /// Self-Contained
  virtual ExiError SC();

private:
  virtual void anchor();
};

class OrderedEncoder;
class ChannelEncoder;

//===----------------------------------------------------------------===//
// ExiEncoder
//===----------------------------------------------------------------===//

struct EncoderFlags {
  /// If the header options were validated.
  bool ValidHeader : 1 = false;
  /// If the header has already been "written".
  bool DidHeader : 1 = false;
  /// If init has already been run.
  bool DidInit : 1 = false;
};

/// The EXI encoding processor.
class ExiEncoder {
  /// The provided Header.
  // TODO: Check .HasOptions everywhere?
  ExiHeader Header = {};
  /// The schema for the current encoder.
  Box<encode::Schema> CurrentSchema;
  /// The encoder, the type of which is determined by the header.
  Box<BodyEncoder> TheEncoder;
  /// State of the decoder in terms of progression.
  EncoderFlags Flags = {};

private:
  struct assumed_valid_tag {};
  ExiEncoder(assumed_valid_tag, MaybeBox<ExiOptions>&& Opts) {
    Header.Opts = std::move(Opts);
    Flags.ValidHeader = true;
  }

public:
  ExiEncoder(MaybeBox<ExiOptions>&& Opts);
  ExiEncoder(ExiEncoder&&) = default;
  ~ExiEncoder();

  /// Get the state flags.
  EncoderFlags flags() const { return Flags; }
  /// Returns if the header was successfully decoded.
  bool didHeader() const { return Flags.DidHeader && Flags.ValidHeader; }

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Creates a new `ExiEncoder` if options are valid.
  static ExiResult<ExiEncoder> New(MaybeBox<ExiOptions>&& Opts);

  /// Sets options for encoding.
  ExiError setOptions(MaybeBox<ExiOptions>&& Opts);
};

} // namespace exi
