//===- exi/Grammar/BIEventMap.hpp -----------------------------------===//
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
/// This file provides a type which can build event maps for builtin encoders.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/MMatch.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/EventTerms.hpp>
#include <exi/Grammar/BIBuilder.hpp>

#define DEBUG_TYPE "BuiltinSchema"

namespace exi {

class BIEventMapBuilder;

/// Used for mapping events to values.
/// Works via "perfect hashing" over the domain.
/// Given:
/// ```
///   SE: 0b0000
///   CM: 0b0101
///   PI: 0b0110
///   DT: 0b0111
///   ED: 0b1010
/// ```
/// `DocContent` can take `0bxx[XX]`.
/// `DocEnd` can take `0bx[XX]x - 1`.
class BIEventMap {
  friend class BIEventMapBuilder;
  /// Accessed with [SE, CM, PI, DT]
  Array<FullEventCode, 4> DocContent {};
  /// Accessed with [ED, CM, PI]
  Array<FullEventCode, 3> DocEnd {};
  /// Currently stores everything. Will change this later.
  using FatElementArray = EnumArray<SecondLevelEventCode, SimpleEventTerm>;
  /// Accessed with [EE, AT, NS, SC, CC...]
  FatElementArray StartTagContent {};
  /// Accessed with [EE, CC...]
  FatElementArray ElementContent {};

  static unsigned GetShift(const ExiOptions& Opts) {
    return (Opts.Alignment == AlignKind::BitPacked) ? 1 : 8;
  }

  constexpr BIEventMap() = default;
public:
  inline static BIEventMap New(const ExiOptions& Opts);

  static constexpr unsigned IdxDocContent(SimpleEventTerm K) {
    using enum SimpleEventTerm;
    exi_invariant(MMatch(K).is(SE, CM, PI, DT));
    return unsigned(K) & 0b0011;
  }
  static constexpr unsigned IdxDocEnd(SimpleEventTerm K) {
    using enum SimpleEventTerm;
    exi_invariant(MMatch(K).is(ED, CM, PI));
    return ((unsigned(K) & 0b0110) >> 1) - 1u;
  }

  constexpr EXI_FLATTEN FullEventCode
   mapDocContent(SimpleEventTerm K) const {
    return DocContent[IdxDocContent(K)];
  }
  constexpr EXI_FLATTEN FullEventCode
   mapDocEnd(SimpleEventTerm K) const {
    return DocContent[IdxDocEnd(K)];
  }

  template <SimpleEventTerm K> 
  constexpr EXI_FLATTEN FullEventCode mapDocContent() const {
    constexpr_static unsigned Off = IdxDocContent(K);
    return DocContent[Off];
  }
  template <SimpleEventTerm K> 
  constexpr EXI_FLATTEN FullEventCode mapDocEnd() const {
    constexpr_static unsigned Off = IdxDocEnd(K);
    return DocContent[Off];
  }
};

template <SimpleEventTerm K>
inline constexpr unsigned map_doccontent_v = BIEventMap::IdxDocContent(K);

template <SimpleEventTerm K>
inline constexpr unsigned map_docend_v = BIEventMap::IdxDocEnd(K);

//////////////////////////////////////////////////////////////////////////
// Builder

/// Builds a `BIEventMap` from the given options.
/// Current implementation isn't clean, but should work.
class BIEventMapBuilder {
  using enum SimpleEventTerm;
  using T = BIEventMap;
  BIEventMap TMap {};
  ExiOptions::PreserveOpts Preserve;
  bool SelfContained;
  bool Packed; // Is bit-aligned?
public:
  BIEventMapBuilder(const ExiOptions& Opts) :
   Preserve(Opts.Preserve),
   SelfContained(Opts.SelfContained),
   Packed(Opts.Alignment == AlignKind::BitPacked) {
    this->initFull();
    this->initSecondLevel();
  }

  BIEventMap operator()() const { return TMap; }

private:
  /// These have no dynamic grammars, and can be fully precomputed.
  /// They are also binary only (eg. 1-bit values of 0/1), so are simpler to compute.
  void initFull();
  template <bool> inline void initDocContent();
  template <bool> inline void initDocEnd();

  /// These have dynamic grammars, and can only be partially precomputed.
  void initSecondLevel();
  inline void initStartTagContent();
  inline void initElementContent();
  inline void initCCItems(BIEventMap::FatElementArray& A,
                          unsigned Count, unsigned Bits);

  bool hasThirdLevelCodes() const {
    return Preserve.PIs || Preserve.Comments;
  }
  bool hasCMPI() const {
    return Preserve.PIs && Preserve.Comments;
  }

  unsigned countCCItemsBits() const {
    unsigned Count = 2;            // {SE, CH}
    Count += Preserve.DTDs;        // ER
    Count += hasThirdLevelCodes(); // {CM, PI}
    return Count;
  }
};

BIEventMap BIEventMap::New(const ExiOptions& Opts) {
  BIEventMapBuilder EMB(Opts);
  return EMB();
}

} // namespace exi

#undef DEBUG_TYPE
