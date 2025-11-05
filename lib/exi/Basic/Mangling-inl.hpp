//===- exi/Basic/Mangling-inl.hpp -----------------------------------===//
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
/// This file implements mangling for ExiOptions & ExiHeader.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/EnumArray.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/ExiHeader.hpp>
#include <exi/Basic/ExiOptions.hpp>

namespace exi {

/// Checks if any Preserves exist.
inline bool opts_have_any_preserve(ExiOptions::PreserveOpts P) {
  return P.Comments      ||
         P.DTDs          ||
         P.LexicalValues ||
         P.PIs           ||
         P.Prefixes;
}

//////////////////////////////////////////////////////////////////////////
// Mangling

#define DEBUG_TYPE "Mangling"

/// Implements option mangling for generic types.
inline void MangleOptions(const ExiOptions& Opts, raw_ostream& OS) {
  // Alignment:
  //  i: bIt
  //  y: bYte
  //  p: Precompression
  //  c: Compression (implies p)
  static constexpr char Alignment[5] {'n','i','y','p','c'};
  OS << Alignment[u8(Opts.Alignment) + u8(Opts.Compression)];
  
  // Strict:
  //  S: true
  // Otherwise ommitted
  if (Opts.Strict)
    OS << 'S';
  
  // SelfContained:
  //  C: true
  // Otherwise ommitted
  if (Opts.SelfContained)
    OS << 'C';
  
  // Preserve:
  //  P, then:
  //   c: Comments
  //   d: DTDs
  //   l: LexicalValues
  //   i: PIs
  //   p: Prefixes
  if (opts_have_any_preserve(Opts.Preserve)) {
    const auto P = Opts.Preserve;
    OS << 'P';
    if (P.Comments)
      OS << 'c';
    if (P.DTDs)
      OS << 'd';
    if (P.LexicalValues)
      OS << 'l';
    if (P.PIs)
      OS << 'i';
    if (P.Prefixes)
      OS << 'p';
  }

  // SchemaID:
  //  Y<N><ID>: exists
  //  Otherwise ommitted
  if (Opts.SchemaID) {
    if (auto& ID = *Opts.SchemaID) {
      LOG_WARN("Schemas IDs unimplemented!");
      OS << 'Y' << ID->size() << format("{:@<{}}", "", ID->size());
    }
  }

  // BlockSize:
  //  B<N>: size
  //  Otherwise ommitted
  if (Opts.Compression)
    // TODO: Use base64?
    OS << 'B' << Opts.BlockSize;
  
  // ValueMaxLength:
  //  If bounded: 
  //   M<N>: length (M if compressed)
  //  Otherwise ommitted
  if (Opts.ValueMaxLength.bounded())
    // TODO: Use base64?
    OS << 'M' << *Opts.ValueMaxLength;

  // ValuePartitionCapacity:
  //  If bounded:
  //   P<N>: length (P if compressed or has max length)
  //  Otherwise ommitted
  if (Opts.ValuePartitionCapacity.bounded())
    // TODO: Use base64?
    OS << 'P' << *Opts.ValuePartitionCapacity;
  
  // TODO: Add options for DTRMap
}

/// Implements header mangling for generic types.
inline void MangleHeader(const ExiHeader& Header, raw_ostream& OS) {
  // HasCookie:
  //  C: yes
  //  Otherwise ommitted
  if (Header.HasCookie)
    OS << 'C';
  
  // ExiVersion:
  //  0<N>: preview
  //      : version 1 (ommitted)
  //   <N>: version N
  if (Header.ExiVersion > 1 || Header.IsPreviewVersion) {
    if (Header.IsPreviewVersion)
      OS << '0';
    if (Header.ExiVersion > 1)
      OS << Header.ExiVersion;
  }

  // HasOptions:
  //  O<...>: yes
  //  N: no
  if (Header.Opts) {
    OS << 'O';
    MangleOptions(*Header.Opts, OS);
  } else
    OS << 'N';
}

#undef DEBUG_TYPE

//////////////////////////////////////////////////////////////////////////
// Demangling

#define DEBUG_TYPE "Demangling"

/// Implements option demangling for generic types.
inline bool DemangleOptions(ExiOptions& Opts, StrRef Sym) {
  if (Sym.empty()) {
    LOG_WARN("Expected characters!");
    return false;
  }

  Opts.Compression = false;
  switch (Sym[0]) {
  case 'i':
    Opts.Alignment = AlignKind::BitPacked;
    break;
  case 'y':
    Opts.Alignment = AlignKind::BytePacked;
    break;
  case 'c':
    Opts.Compression = true;
    [[fallthrough]];
  case 'p':
    Opts.Alignment = AlignKind::PreCompression;
    break;
  default:
    LOG_WARN("Unexpected character: '{}'", Sym[0]);
    return false;
  }
  
  Opts.Strict = Sym.consume_front("S");
  Opts.SelfContained = Sym.consume_front("C");

  Opts.Preserve = {};
  if (Sym.consume_front("P")) {
    auto& P = Opts.Preserve;
    if (Sym.consume_front("c"))
      P.Comments = true;
    if (Sym.consume_front("d"))
      P.DTDs = true;
    if (Sym.consume_front("l"))
      P.LexicalValues = true;
    if (Sym.consume_front("i"))
      P.PIs = true;
    if (Sym.consume_front("p"))
      P.Prefixes = true;
  }
  
  if (Sym.consume_front("Y")) {
    LOG_WARN("Schemas IDs unimplemented!");
    usize Skip = 0;
    if (Sym.consumeInteger(10, Skip)) {
      LOG_WARN("Invalid SchemaID size.");
      return false;
    }
    Sym = Sym.drop_front(Skip);
  }

  if (Opts.Compression)
    if (Sym.consume_front("B"))
      if (Sym.consumeInteger(10, Opts.BlockSize)) {
        LOG_WARN("Expected integer BlockSize.");
        return false;
      }

  Opts.ValueMaxLength = unbounded;
  if (Sym.consume_front("M")) {
    u64 ValueMaxLength = 0;
    Sym.consumeInteger(10, ValueMaxLength);
    if (Sym.consumeInteger(10, ValueMaxLength)) {
      LOG_WARN("Expected integer ValueMaxLength.");
      return false;
    }
    Opts.ValueMaxLength = ValueMaxLength;
  }

  Opts.ValuePartitionCapacity = unbounded;
  if (Sym.consume_front("P")) {
    u64 ValuePartitionCapacity = 0;
    Sym.consumeInteger(10, ValuePartitionCapacity);
    if (Sym.consumeInteger(10, ValuePartitionCapacity)) {
      LOG_WARN("Expected integer ValuePartitionCapacity.");
      return false;
    }
    Opts.ValuePartitionCapacity = ValuePartitionCapacity;
  }

  return true;
}

/// Implements header demangling for generic types.
inline bool DemangleHeader(ExiHeader& Header, StrRef Sym) {
  Header.HasCookie = Sym.consume_front("C");

  // ExiVersion:
  //  0<N>: preview
  //      : version 1 (ommitted)
  //   <N>: version N
  Header.IsPreviewVersion = Sym.consume_front("0");
  unsigned Version = 1;
  Sym.consumeInteger(10, Version);
  Header.ExiVersion = Version;
  
  if (Sym.consume_front("O")) {
    if (!Header.Opts)
      Header.Opts = std::make_unique<ExiOptions>();
    return DemangleOptions(*Header.Opts, Sym);
  }

  if (Sym.consume_front("N"))
    return true;

  if (!Sym.empty()) {
    LOG_WARN("Unexpected trailing characters: \"{}\"", Sym);
    return false;
  }

  LOG_WARN("Unexpected end!");
  return false;
}

#undef DEBUG_TYPE

} // namespace exi
