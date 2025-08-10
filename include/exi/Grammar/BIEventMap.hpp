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

// SE == 0x0
// EE == 0x1
// AT == 0x2
// CH == 0x3
// NS == 0x4
// SD == 0x5
// ED == 0x6
// CM == 0x7
// PI == 0x8
// DT == 0x9
// ER == 0xA

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
  /// Accessed with [SE, CM, PI, DT]
  Array<FullEventCode, 4> DocContent {};
  /// Accessed with [ED, CM, PI]
  Array<FullEventCode, 3> DocEnd {};

  static unsigned GetShift(const ExiOptions& Opts) {
    return (Opts.Alignment == AlignKind::BitPacked) ? 1 : 8;
  }

  BIEventMap(ExiOptions::PreserveOpts Preserve, unsigned Shift) {
    // TODO: Implement
  }

public:
  BIEventMap() = default; // FIXME: Remove
  BIEventMap(const ExiOptions& Opts)
   : BIEventMap(Opts.Preserve, GetShift(Opts)) {
  }

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

} // namespace exi

#undef DEBUG_TYPE
