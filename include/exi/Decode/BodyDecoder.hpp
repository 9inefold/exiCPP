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
#include <core/Common/Vec.hpp>
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
  /// If the stream was set externally.
  bool SetReader : 1 = false;
  /// If the header has already been "parsed".
  bool DidHeader : 1 = false;
  /// If init has already been run.
  bool DidInit : 1 = false;
};

/// The EXI decoding processor.
class ExiDecoder {
  friend class Schema::Get;

  /// The provided Header.
  ExiHeader Header;
  /// The provided `StreamReader`.
  Poly<BitStreamReader, ByteStreamReader> Reader;
  /// A BumpPtrAllocator for processor internals.
  exi::BumpPtrAllocator BP;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  decode::StringTable Idents;
  /// The schema for the current document.
  /// TODO: Add SchemaResolver...
  Box<Schema> CurrentSchema;
  /// The stack of current grammars.
  SmallVec<const InlineStr*> GrammarStack;

  /// The stream used for diagnostics.
  Option<raw_ostream&> OS;
  /// State of the decoder in terms of progression.
  DecoderFlags Flags;
  /// Preserve options.
  ExiOptions::PreserveOpts Preserve;

public:
  ExiDecoder(Option<raw_ostream&> OS = std::nullopt) : OS(OS) {}
  ExiDecoder(MaybeBox<ExiOptions> Opts, Option<raw_ostream&> OS = std::nullopt);
  ~ExiDecoder() { os().flush(); }

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

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Returns an error if the reader is empty.
  ExiError readerExists() const;
  /// Sets options out-of-band.
  ExiError setOptions(MaybeBox<ExiOptions> Opts);
  /// Sets reader out-of-band. Options must be provided.
  ExiError setReader(UnifiedBuffer Buffer);

  /// Decodes the header from the provided buffer.
  /// Defined in `HeaderDecoder.cpp`.
  ExiError decodeHeader(UnifiedBuffer Buffer);
  /// Decodes the body from the current stream.
  ExiError decodeBody();

protected:
  /// Initializes StringTable and Schema.
  ExiError init();
  ExiError decodeEvent();

  ////////////////////////////////////////////////////////////////////////
  // Terms

  ExiError handleSE(EventUID Event);
  ExiError handleEE(EventUID Event);
  ExiError handleAT(EventUID Event);
  ExiError handleNS(EventUID Event);
  ExiError handleCH(EventUID Event);

  StrRef getPfxOrURI(EventUID Event);
  Option<StrRef> tryGetPfx(CompactID URI, CompactID PfxID);

  ////////////////////////////////////////////////////////////////////////
  // Values

  /// Decodes a QName.
  ExiResult<EventUID> decodeQName();

  /// Decodes a Namespace.
  ExiResult<EventUID> decodeNS();

  /// Decodes a QName URI.
  ExiResult<CompactID> decodeURI();

  /// Decodes a QName LocalName.
  /// @param URI The bucket to search in.
  ExiResult<CompactID> decodeName(CompactID URI);

  /// Same as `decodeName`, decodes a QName LocalName.
  ALWAYS_INLINE auto decodeLocalName(CompactID URI) {
    return this->decodeName(URI);
  }

  /// Decodes a QName Prefix, if `Preserve.Prefixes` is enabled.
  /// @param URI The bucket to search in.
  ExiResult<Option<CompactID>> decodePfxQ(CompactID URI);

  /// Decodes a NS Prefix, `Preserve.Prefixes` must be enabled.
  /// @param URI The bucket to search in.
  ExiResult<CompactID> decodePfx(CompactID URI);

  /// Decodes a Value.
  ExiResult<EventUID> decodeValue(CompactID URI, CompactID Name) {
    return this->decodeValue(SmallQName::MakeQName(URI, Name));
  }
  /// Decodes a Value.
  ExiResult<EventUID> decodeValue(SmallQName Name);

  /// @brief Decodes an encoded string with the default character set.
  /// @return An owning `String`, or an error.
  /// @overload
  ExiResult<String> decodeString();

  /// @brief Decodes an encoded string with the default character set.
  /// @param Storage Where the string will be stored.
  /// @return An non-owning `StrRef`, or an error.
  ExiResult<StrRef> decodeString(SmallVecImpl<char>& Storage);

  /// @brief Decodes a string with with the size already decoded.
  /// @param Size The length of the string.
  /// @param Storage Where the string will be stored.
  /// @return An non-owning `StrRef`, or an error.
  ExiResult<StrRef> readString(u64 Size, SmallVecImpl<char>& Storage);
};

} // namespace exi
