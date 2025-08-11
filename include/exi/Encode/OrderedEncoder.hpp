//===- exi/Encode/OrderedEncoder.hpp --------------------------------===//
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
/// This file implements an exi processor for in-order writers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <exi/Basic/XML.hpp>
#include <exi/Encode/BodyEncoder.hpp>
#include <exi/Encode/StringTable.hpp>
#include <exi/Encode/NamespaceContextStack.hpp>
#include <exi/Grammar/EncoderSchema.hpp>
#include <exi/Stream/OrderedWriter.hpp>

namespace exi {

class OrderedEncoder final : public BodyEncoder {
  friend class ExiEncoder;
  friend struct encode::Schema::Get<BitWriter>;
  friend struct encode::Schema::Get<ByteWriter>;

  using BodyEncoder::Opts;
  /// A BumpPtrAllocator for processor internals.
  exi::BumpPtrAllocator BP;
  /// The provided `OrderedWriter`.
  OrdWriter Writer;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  encode::StringTable Strings;
  /// The stack of namespace contexts.
  NSContextStack CtxStack;
  /// The schema for the current document.
  Box<encode::Schema> CurrentSchema;

  bool IsConstructed : 1 = false;
  bool IsStreamInitialized : 1 = false;
  bool DidEncodeHeader : 1 = false;

  /// Creates a new schema instance if initialized.
  Box<encode::Schema> makeSchemaFromThis(encode::factory_t& F) {
    if EXI_UNLIKELY(!F)
      return nullptr;
    return F(this);
  }

public:
  OrderedEncoder(ExiOptions& Opts, encode::factory_t& F, ExiError* E = nullptr);
  /// Initializes the writer with a stream/buffer.
  template <typename InitT>
  OrderedEncoder(ExiOptions& Opts, encode::factory_t& F,
    InitT& I, ExiError* E = nullptr)
   : OrderedEncoder(Opts, F, E) {
    if (auto Err = init(I)) [[unlikely]]
      if (E != nullptr)
        *E = std::move(Err);
  }

  EXI_INLINE EXI_FLATTEN encode::Schema* getSchema() const {
    return CurrentSchema.operator->();
  }

  static bool classof(const BodyEncoder* BE) {
    return BE->get_kind() == EncoderKind::EK_Ordered;
  }

  ////////////////////////////////////////////////////////////////////////
  // Initialization

  /// Returns an error if the writer isn't empty.
  ExiError assumeWriterIsEmpty() const;
  /// Generic interface for initializing the OrderedWriter.
  ExiError init(raw_ostream& Strm);
  /// Generic interface for initializing the OrderedWriter.
  ExiError init(SmallVecImpl<char>& Buf);

private:
  /// Initializes the writer with arbitrary data.
  void initWriter(auto& BufOrStrm, auto...Rest) {
    if (Opts.Alignment == AlignKind::BitPacked) {
      Writer.emplace<BitWriter>(BufOrStrm, Rest...);
    } else /*AlignKind::BytePacked*/ {
      Writer.emplace<ByteWriter>(BufOrStrm, Rest...);
    }
    IsStreamInitialized = true;
  }

public:
  /// Checks that a `BitBuffer` could contain valid data.
  inline static bool IsValidHeaderBuffer(BitBuffer Data,
                                         AlignKind A = AlignKind::BitPacked);
  /// Checks if the data should be written to the buffer.
  /// @returns A value if no, otherwise `None`.
  inline Option<ExiError> shouldEncodeHeader(BitBuffer Data,
                                             bool KnownValid = false) const;
  /// Writes the header to the provided stream.
  inline ExiError encodeHeader(BitBuffer Data, bool KnownValid = false);
  /// Writes the header to the provided stream.
  ExiError encodeHeader(BitBuffer Data) override;

  bool isReady() const override {
    return IsConstructed
        && IsStreamInitialized
        && DidEncodeHeader;
  }

  ////////////////////////////////////////////////////////////////////////
  // Terms

  ExiError StartDocument() {
    static constexpr StartDocEvent SD
      = make_event<SimpleEventTerm::SD>();
    return getSchema()->encode(this, SD);
  }
  ExiError EndDocument() {
    static constexpr EndDocEvent ED
      = make_event<SimpleEventTerm::ED>();
    return getSchema()->encode(this, ED);
  }

  ExiError Comment(StrRef Data) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::CM>(Data));
  }
  ExiError ProcessingInstruction(StrRef Name, StrRef Data) {
    return getSchema()->encode(this,
      make_event<SimpleEventTerm::PI>(Name, Data));
  }
  ExiError Doctype(const DoctypeEvent& DocType) {
    return getSchema()->encode(this, DocType);
  }
};

} // namespace exi
