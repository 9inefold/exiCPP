//===- exi/Grammar/BIEventMap.hpp -----------------------------------===//
//
// Copyright (C) 2025 Ninefold
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

class BodyEncoder;
class BIEventMapBuilder;

namespace eventmap {

using TermT = SimpleEventTerm;

inline constexpr unsigned DocContentIdx(TermT K) {
  using enum SimpleEventTerm;
  exi_invariant(mmatch_value(K).is(SE, CM, PI, DT));
  return unsigned(K) & 0b0011;
}
inline constexpr unsigned DocEndIdx(TermT K) {
  using enum SimpleEventTerm;
  exi_invariant(mmatch_value(K).is(ED, CM, PI));
  return ((unsigned(K) & 0b0110) >> 1) - 1u;
}

/// Maps full event codes for `DocContent`.
struct DocContentArray {
  FullEventCode Data[4] {};
public:
  constexpr EXI_FLATTEN EXI_INLINE FullEventCode& operator[](TermT K) {
    return Data[DocContentIdx(K)];
  }
  constexpr EXI_FLATTEN EXI_INLINE FullEventCode operator[](TermT K) const {
    return Data[DocContentIdx(K)];
  }
};

/// Maps full event codes for `DocEnd`.
struct DocEndArray {
  FullEventCode Data[3] {};
public:
  constexpr EXI_FLATTEN EXI_INLINE FullEventCode& operator[](TermT K) {
    return Data[DocEndIdx(K)];
  }
  constexpr EXI_FLATTEN EXI_INLINE FullEventCode operator[](TermT K) const {
    return Data[DocEndIdx(K)];
  }
};

/// Currently stores everything. Will change this later.
using FatElementArray = EnumArray<SecondLevelEventCode, SimpleEventTerm>;

using StartTagArray = FatElementArray;
using ElementArray  = FatElementArray;

} // namespace eventmap

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
  eventmap::DocContentArray DocContent;
  /// Accessed with [ED, CM, PI]
  eventmap::DocEndArray DocEnd;
  /// Accessed with [EE, AT, NS, SC, CC...]
  eventmap::StartTagArray StartTagContent {};
  /// Accessed with [EE, CC...]
  eventmap::ElementArray ElementContent {};

  static unsigned GetShift(const ExiOptions& Opts) {
    return (Opts.Alignment == AlignKind::BitPacked) ? 1 : 8;
  }
  constexpr BIEventMap() = default;

public:
  [[nodiscard]] inline static BIEventMap New(const ExiOptions& Opts);

  [[nodiscard]] constexpr FullEventCode
   mapDocContent(SimpleEventTerm K) const {
    return DocContent[K];
  }
  [[nodiscard]] constexpr FullEventCode
   mapDocEnd(SimpleEventTerm K) const {
    return DocEnd[K];
  }
  [[nodiscard]] constexpr EXI_FLATTEN SecondLevelEventCode
   mapStartTagContent(SimpleEventTerm K) const {
    return StartTagContent[K];
  }
  [[nodiscard]] constexpr EXI_FLATTEN SecondLevelEventCode
   mapElementContent(SimpleEventTerm K) const {
    return ElementContent[K];
  }

  template <SimpleEventTerm K> 
  [[nodiscard]] constexpr FullEventCode mapDocContent() const {
    constexpr unsigned Off = eventmap::DocContentIdx(K);
    return DocContent.Data[Off];
  }
  template <SimpleEventTerm K> 
  [[nodiscard]] constexpr FullEventCode mapDocEnd() const {
    constexpr unsigned Off = eventmap::DocEndIdx(K);
    return DocEnd.Data[Off];
  }
  template <SimpleEventTerm K> 
  [[nodiscard]] constexpr EXI_FLATTEN
   SecondLevelEventCode mapStartTagContent() const {
    return StartTagContent[K];
  }
  template <SimpleEventTerm K> 
  [[nodiscard]] constexpr EXI_FLATTEN
   SecondLevelEventCode mapElementContent() const {
    return ElementContent[K];
  }

#if EXI_DEBUG || EXI_ENABLE_DUMP
  EXI_DUMP_METHOD void dump(BodyEncoder* BE) const;
  EXI_DUMP_METHOD void dump(const ExiOptions& Opts) const;
private:
  inline void dumpImpl(const ExiOptions& Opts, bool IsPacked) const;
#endif
};

template <SimpleEventTerm K>
inline constexpr unsigned map_doccontent_v = eventmap::DocContentIdx(K);

template <SimpleEventTerm K>
inline constexpr unsigned map_docend_v = eventmap::DocEndIdx(K);

//////////////////////////////////////////////////////////////////////////
// Builder

/// Builds a `BIEventMap` from the given options.
/// Current implementation isn't clean, but should work.
class BIEventMapBuilder {
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
  inline void initCCItems(eventmap::FatElementArray& A,
                          unsigned Count, unsigned Bits);
};

BIEventMap BIEventMap::New(const ExiOptions& Opts) {
  BIEventMapBuilder EMB(Opts);
  return EMB();
}

} // namespace exi

#undef DEBUG_TYPE
