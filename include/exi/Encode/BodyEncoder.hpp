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
#include <exi/Basic/BitBuffer.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Grammar/EncoderSchema.hpp>
#include <exi/Grammar/SchemaFactory.hpp>

namespace exi {

class BodyEncoder;
class OrderedEncoder;
class ChannelEncoder;

//===----------------------------------------------------------------===//
// ExiEncoder
//===----------------------------------------------------------------===//

class Serializer;

struct EncoderFlags {
  /// If the header options were validated.
  bool ValidHeader : 1 = false;
  /// If the header has already been "written".
  bool DidHeader : 1 = false;
  /// If init has already been run.
  bool DidInit : 1 = false;
};

/// The EXI encoding processor generator. You configure your options here, then
/// create an `EncoderFactory` that will instantiate the encoder for you.
class ExiEncoder {
  /// The provided Header.
  // TODO: Check .HasOptions everywhere?
  ExiHeader Header = {};
  /// The SchemaFactory for the current encoder.
  encode::factory_t ESFactory = nullptr;
  /// A buffer containing the precompiled options.
  Option<OwningBitBuffer> PCH = std::nullopt;
  /// State of the decoder in terms of progression.
  EncoderFlags Flags = {};

private:
  struct assumed_valid_tag {};
  ExiEncoder(assumed_valid_tag, MaybeBox<ExiOptions>&& Opts) {
    Header.Opts = std::move(Opts);
    Flags.ValidHeader = true;
  }

public:
  ExiEncoder(MaybeBox<ExiOptions>&& Opts, ExiError* Err = nullptr);
  ExiEncoder(ExiEncoder&&) = default;
  ~ExiEncoder();

  /// Gets options, if they exist.
  Option<const ExiOptions&> getOptions() const {
    if (!Header.Opts)
      return std::nullopt;
    return *Header.Opts;
  }
  /// Gets the precompiled header, if it exists.
  Option<BitBuffer> getPCH() const { return PCH; }
  /// Get the state flags.
  EncoderFlags flags() const { return Flags; }
  /// Returns if the header was successfully decoded.
  bool didHeader() const { return Flags.DidHeader && Flags.ValidHeader; }

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  class EncoderFactory {
    friend class ExiEncoder;
    /// The bound instance.
    ExiEncoder* This;
    /// The encoder, the type of which is determined by the header.
    Box<BodyEncoder> TheEncoder = nullptr;
    /// Only ExiEncoder can create an instance.
    EncoderFactory(ExiEncoder* This) : This(This) {}
  public:
    /// Generates the encoder and runs.
    ExiError encode(Serializer* S, raw_ostream& Strm) EXI_NONNULL(2);
    /// Generates the encoder and runs.
    ExiError encode(Serializer* S, SmallVecImpl<char>& Buf) EXI_NONNULL(2);
  private:
    inline ExiError encodeGeneric(Serializer* S, auto& I);
    /// Runs the encoder.
    ExiError go(Serializer* S) const;
  };

  /// Creates a new `ExiEncoder` if options are valid.
  static ExiResult<ExiEncoder> New(MaybeBox<ExiOptions>&& Opts);
  /// Sets options for encoding.
  ExiError setOptions(MaybeBox<ExiOptions>&& Opts);
  /// Sets whether the $EXI cookie will be encoded.
  ExiError hdrHasCookie(bool HasCookie);
  /// Sets whether options will be encoded or out-of-band.
  ExiError hdrHasOptions(bool IncludeOptions);
  /// Sets whether the $EXI cookie will be encoded.
  ExiError hdrVersion(u32 Version = kCurrentExiVersion)
   EXI_ERROR_IF(Version > kCurrentExiVersion, "Invalid EXI version!");

  /// Precompiles header to a `BitBuffer`.
  /// Defined in `HeaderEncoder.cpp`.
  /// @param IncludeOptions If options should be encoded as well.
  ExiError compileHeader(
    Option<bool> IncludeOptions = std::nullopt);
  /// Creates the `EncoderFactory` for the current setup.
  ExiResult<EncoderFactory> setup(
    Option<bool> IncludeOptions = std::nullopt);

private:
  /// Initializes Schema.
  ExiError init();
};

//===----------------------------------------------------------------===//
// BodyEncoder
//===----------------------------------------------------------------===//

/// The top-level interface for encoder implementations.
class BodyEncoder {
public:
  enum class EncoderKind {
    EK_Generic,
    EK_Ordered,
    EK_Channel,
  };

protected:
  EncoderKind Kind;
  /// The options for the current encoding
  const ExiOptions& Opts;

public:
  BodyEncoder(ExiOptions& Opts,
              EncoderKind K = EncoderKind::EK_Generic)
      : Kind(K), Opts(Opts) {}
  virtual ~BodyEncoder() = default;

  /// Writes the header to the provided stream.
  virtual ExiError encodeHeader(BitBuffer Data) = 0;
  /// Checks the stream is ready to go.
  virtual bool isReady() const = 0;

  /// Gets the options held by the current encoder.
  const ExiOptions& getOptions() const { return Opts; }
  /// Gets the value of Preserve.Prefixes.
  bool PreservePrefixes() const { return Opts.Preserve.Prefixes; }
  /// Gets the value of Preserve.LexicalValues.
  bool LexicalValues() const { return Opts.Preserve.LexicalValues; }
  /// Gets the type of the encoder.
  EncoderKind kind() const { return Kind; }
  /// Gets the type of the stream.
  virtual StreamBase::StreamKind streamKind() const = 0;

  ////////////////////////////////////////////////////////////////////////
  // Terms
  
  [[nodiscard]] static std::pair<StrRef, StrRef> SplitName(StrRef S) {
    usize Idx = S.find(':');
    if (Idx == StrRef::npos)
      return std::make_pair(""_str, S);
    return std::make_pair(S.slice(0, Idx), S.substr(Idx + 1));
  }

private:
  virtual void anchor();
};

} // namespace exi
