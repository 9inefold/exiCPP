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
#include <exi/Stream/OrderedWriter.hpp>
#include <exi/Grammar/EncoderSchema.hpp>
#include <exi/Stream/OrderedWriter.hpp>

namespace exi {

class OrderedEncoder final : public BodyEncoder {
  friend class ExiEncoder;
  /// The provided `OrderedWriter`.
  OrdWriter Writer;
  /// A BumpPtrAllocator for processor internals.
  exi::BumpPtrAllocator BP;
  /// The table holding decoded string values (QNames, LocalNames, etc.)
  encode::StringTable Idents;
  /// The schema for the current document.
  encode::Schema* CurrentSchema = nullptr;

public:
  OrderedEncoder(ExiOptions& Opts, encode::Schema* CS);
  /// Initializes the writer with a stream/buffer.
  template <typename InitT>
  OrderedEncoder(ExiOptions& Opts, encode::Schema* CS, InitT& I)
   : OrderedEncoder(Opts, CS) { this->init(I); }

  static bool classof(const BodyEncoder* BE) {
    return BE->get_kind() == EncoderKind::EK_Ordered;
  }

  /// Generic interface for initializing the OrderedWriter.
  ExiError init(raw_ostream& Strm);
  /// Generic interface for initializing the OrderedWriter.
  ExiError init(SmallVecImpl<char>& Buf);

  /// Writes the header to the provided stream.
  ExiError encodeHeader(BitBuffer Data) override {
    Writer->writeBitBuffer(Data);
    return ExiError::OK;
  }

private:
  void initWriter(auto&&...Args) {
    if (Opts.Alignment == AlignKind::BitPacked) {
      Writer.emplace<BitWriter>(EXI_FWD(Args)...);
    } else /*AlignKind::BytePacked*/ {
      Writer.emplace<ByteWriter>(EXI_FWD(Args)...);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // Terms
public:

};

} // namespace exi
