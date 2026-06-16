//===- exi/Grammar/BIEventMap.cpp -----------------------------------===//
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

#include <exi/Grammar/BIEventMap.hpp>
//#include <core/Common/STLExtras.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/WithColor.hpp>
#include <exi/Encode/BodyEncoder.hpp>
#include <array>

#define DEBUG_TYPE "BuiltinSchema"

using namespace exi;
using enum SimpleEventTerm;

static_assert(i32(SE) == 0x0, "Invalid assumption: SE");
static_assert(i32(EE) == 0x1, "Invalid assumption: EE");
static_assert(i32(AT) == 0x2, "Invalid assumption: AT");
static_assert(i32(CH) == 0x3, "Invalid assumption: CH");
static_assert(i32(NS) == 0x4, "Invalid assumption: NS");
static_assert(i32(CM) == 0x5, "Invalid assumption: CM");
static_assert(i32(PI) == 0x6, "Invalid assumption: PI");
static_assert(i32(DT) == 0x7, "Invalid assumption: DT");
static_assert(i32(ER) == 0x8, "Invalid assumption: ER");
static_assert(i32(SD) == 0x9, "Invalid assumption: SD");
static_assert(i32(ED) == 0xA, "Invalid assumption: ED");
static_assert(i32(SC) == 0xB, "Invalid assumption: SC");

static bool HasThirdLevelCodes(ExiOptions::PreserveOpts Preserve) {
  return Preserve.PIs || Preserve.Comments;
}

static bool HasCMPI(ExiOptions::PreserveOpts Preserve) {
  return Preserve.PIs && Preserve.Comments;
}

static unsigned CountCCItemsBits(ExiOptions::PreserveOpts Preserve) {
  unsigned Count = 2;                    // {SE, CH}
  Count += Preserve.DTDs;                // ER
  Count += HasThirdLevelCodes(Preserve); // {CM, PI}
  return Count;
}

#if EXI_DEBUG || EXI_ENABLE_DUMP
static auto FormatTerm(SimpleEventTerm Term) {
  return left_justify(get_event_fullname(Term), 8);
}

//////////////////////////////////////////////////////////////////////////
// dumpImpl

namespace {
/// The list of event terms.
using TermListType = std::initializer_list<SimpleEventTerm>;
/// The list of event terms.
using TermListRef = exi::ArrayRef<SimpleEventTerm>;
/// The array data.
using CountsType = std::array<u8, 3>;

/// Handles dumping data for the event map.
class DumpImpl {
  ExiOptions::PreserveOpts Preserve;
  // {StartTagContent, ElementContent}
  CountsType Counts[2] = {};
  bool SelfContained;
  bool IsPacked;

  void initCCCounts(CountsType& C) {
    C[1] += 2; // SE, CH
    C[1] += Preserve.DTDs;
    if (HasCMPI(Preserve)) {
      C[2] = 2;
    } else if (HasThirdLevelCodes(Preserve)) {
      C[1] += 1;
    }
  }

  ALWAYS_INLINE void initArrays() {
    if (!IsPacked) return;
    // StartTagContent
    Counts[0][0] = 0;
    Counts[0][1] = 2;
    Counts[0][1] += Preserve.Prefixes;
    Counts[0][1] += SelfContained;
    initCCCounts(Counts[0]);
    // ElementContent
    Counts[1][0] = 1;
    initCCCounts(Counts[1]);
  }

public:
  inline DumpImpl(const ExiOptions& Opts, bool IsPacked)
   : Preserve(Opts.Preserve), SelfContained(Opts.SelfContained),
     IsPacked(IsPacked) {
    // Set up our count arrays
    this->initArrays();
  }

  /// 2-value code
  void code(FullEventCode C) const;
  /// bIt packed code
  void codeI(u32 First, u32 Level, SecondLevelEventCode C) const;
  /// bYte packed code
  void codeY(u32 First, SecondLevelEventCode C) const;
  /// 2-value packed array
  void array(auto& A, TermListType TermList) const;
  /// bIt packed array
  void arrayI(u32 FirstLevel, auto& A, TermListRef TermList) const;
  /// bYte packed array
  void arrayY(u32 FirstLevel, auto& A, TermListRef TermList) const;
  /// bit/byte packed array dispatcher
  ALWAYS_INLINE void array(u32 FirstLevel, auto& A, TermListType TermList) const {
    if (IsPacked)
      return arrayI(FirstLevel, A, TermList);
    else
      return arrayY(FirstLevel, A, TermList);
  }
};
} // namespace `anonymous`

static constexpr u32 MakeLog32(unsigned NsN) {
  //return NsN ? BISmallLog2[NsN] + 1 : 0;
  return BISmallLog2[NsN];
}
static constexpr u32 MakeMask32(unsigned Bits) {
  exi_assume(Bits < 32);
  return (~u32(0)) >> (32 - Bits);
}

void DumpImpl::code(FullEventCode C) const {
  const unsigned N = IsPacked ? 1 : 8;
  const u32 Mask = IsPacked ? 0b1 : 0xFF;
  // Print 2-value codes
  unsigned Bytes = C.Bits / N;
  if (Bytes == 0)
    return;
  u32 Curr = C.Data >> ((Bytes - 1) * N);
  errs() << (Curr & Mask);
  for (unsigned Ix = 2; Ix <= Bytes; ++Ix) {
    Curr = C.Data >> ((Bytes - Ix) * N);
    errs() << '.' << (Curr & Mask);
  }
}

void DumpImpl::codeI(u32 First, u32 Level, SecondLevelEventCode C) const {
  const auto Ns = Counts[First];
  // Handle [x].y.z
  errs() << First;
  unsigned ShiftBy = BISmallLog2[Ns[0]];
  // Handle x.[y].z & x.y.[z]
  for (unsigned Ix = 1; Ix <= Level; ++Ix) {
    unsigned CurrBits = BISmallLog2[Ns[Ix]];
    ShiftBy += CurrBits;
    u32 Curr = C.Data >> (C.Bits - ShiftBy);
    errs() << '.' << (Curr & MakeMask32(CurrBits));
  }
}

void DumpImpl::codeY(u32 First, SecondLevelEventCode C) const {
  unsigned Bytes = C.Bits / 8;
  errs() << First;
  if (Bytes == 0)
    return;
  for (unsigned Ix = 1; Ix <= Bytes; ++Ix) {
    u32 Curr = C.Data >> ((Bytes - Ix) * 8);
    errs() << '.' << (Curr & 0xFF);
  }
}

void DumpImpl::array(auto& A, TermListType TermList) const {
  // Check if terms are implicit
  const auto FirstTerm = *TermList.begin();
  if (IsPacked && A[FirstTerm].Bits == 0) {
    errs() << "  " << FormatTerm(FirstTerm) << "0\n";
    return;
  }
  // We actually have terms to loop over
  for (auto Term : TermList) {
    FullEventCode C = A[Term];
    if (C.Bits == 0)
      continue;
    errs() << "  " << FormatTerm(Term);
    this->code(C);
    errs() << '\n';
  }
}

void DumpImpl::arrayI(u32 FirstLevel, auto& A, TermListRef TermList) const {
  exi_invariant(FirstLevel <= 1);
  CountsType Ns = Counts[FirstLevel];
  unsigned Level = 1;
  // Iter helper
  auto IterLevel = [&] () -> bool {
    if (Ns[Level] == 0) {
      if (++Level > 2)
        return false;
    }
    Ns[Level] -= 1;
    return true;
  };
  for (auto Term : TermList) {
    SecondLevelEventCode C = A[Term];
    if (C.Bits == 0)
      continue;
    if (!IterLevel())
      return;
    errs() << "  " << FormatTerm(Term);
    this->codeI(FirstLevel, Level, C);
    errs() << '\n';
  }
}

void DumpImpl::arrayY(u32 FirstLevel, auto& A, TermListRef TermList) const {
  for (auto Term : TermList) {
    SecondLevelEventCode C = A[Term];
    if (C.Bits == 0)
      continue;
    errs() << "  " << FormatTerm(Term);
    this->codeY(FirstLevel, C);
    errs() << '\n';
  }
}

void BIEventMap::dumpImpl(const ExiOptions& Opts, bool IsPacked) const {
  DumpImpl D(Opts, IsPacked);
  errs() << "Document:\n  " << FormatTerm(SD) << 0 << "\n";

  errs() << "\nDocContent:\n";
  D.array(DocContent, {SE, DT, CM, PI});

  errs() << "\nDocEnd:\n";
  D.array(DocEnd, {ED, CM, PI});

  errs() << "\nStartTagContent:\n";
  D.array(0, StartTagContent, {EE, AT, NS, SC, SE, CH, ER, CM, PI});

  errs() << "\nElementContent:\n";
  errs() << "  " << FormatTerm(EE) << "0\n";
  D.array(1, ElementContent, {SE, CH, ER, CM, PI});
}

//////////////////////////////////////////////////////////////////////////
// dump

EXI_DUMP_METHOD void BIEventMap::dump(const ExiOptions& Opts) const {
  if (Opts.Alignment == AlignKind::None) {
    LOG_ERROR("AlignKind cannot be None!");
    return;
  } else if (Opts.Fragment) {
    LOG_ERROR("Fragment is unimplemented!");
    return;
  }
  if (Opts.Alignment == AlignKind::BitPacked)
    return this->dumpImpl(Opts, true);
  else
    return this->dumpImpl(Opts, false);
}

EXI_DUMP_METHOD void BIEventMap::dump(BodyEncoder* BE) const {
  if (BE == nullptr) {
    LOG_ERROR("Dumping Encoder cannot be null!");
    return;
  }
  return this->dump(BE->getOptions());
}
#endif

//////////////////////////////////////////////////////////////////////////
// Full

namespace {
template <bool Packed> struct ECGen {
  static constexpr unsigned Off = Packed ? 1 : 8;

  template <bool _1, bool _2, bool _3>
  static constexpr u32 DataRaw =
    (u32(_1) << (2 * Off)) | (u32(_2) << (1 * Off)) | u32(_3);
  
  template <bool _3, bool _2, bool _1, unsigned Level>
  static constexpr u32 Data =
    (DataRaw<_1, _2, _3>) >> ((3 - Level) * Off);

  template <bool _3, bool _2, bool _1, unsigned Level>
  static constexpr FullEventCode v = {
    .Data = Data<_3, _2, _1, Level>,
    .Bits = (Level * Off)
  };
};

template <bool Packed,
  bool _3, bool _2, bool _1, unsigned V>
inline constexpr FullEventCode ecgen_v
  = ECGen<Packed>::template v<_3, _2, _1, V>;
} // namespace `anonymous`

/// Creates a constant FullEventCode
#define X(...) ecgen_v<PACKED, __VA_ARGS__>

template <bool PACKED>
inline void BIEventMapBuilder::initDocContent() {
  eventmap::DocContentArray& A = TMap.DocContent;
  if (!HasThirdLevelCodes(Preserve)) {
    if (Preserve.DTDs) {
      A[SE] = X(0,0,0, 1);
      A[DT] = X(0,0,1, 1);
    }
    return;
  }

  // Set up base level SE(*)
  A[SE] = X(0,0,0, 1);
  if (Preserve.DTDs) {
    A[DT]   = X(0,0,1, 2);
    if (HasCMPI(Preserve)) {
      A[CM] = X(0,1,1, 3);
      A[PI] = X(1,1,1, 3);
    } else if (Preserve.Comments) {
      A[CM] = X(0,1,1, 2);
    } else /*Preserve.PIs*/ {
      A[PI] = X(0,1,1, 2);
    }
  } else if (HasCMPI(Preserve)) {
    A[CM]   = X(0,0,1, 2);
    A[PI]   = X(0,1,1, 2);
  } else if (Preserve.Comments) {
    A[CM]   = X(0,0,1, 1);
  } else /*Preserve.PIs*/ {
    A[PI]   = X(0,0,1, 1);
  }
}

template <bool PACKED>
inline void BIEventMapBuilder::initDocEnd() {
  if (!HasThirdLevelCodes(Preserve))
    return;
  
  eventmap::DocEndArray& A = TMap.DocEnd;
  // Set up base level ED
  A[ED]   = X(0,0,0, 1);
  if (HasCMPI(Preserve)) {
    A[CM] = X(0,0,1, 2);
    A[PI] = X(0,1,1, 2);
  } else if (Preserve.Comments) {
    A[CM] = X(0,0,1, 1);
  } else /*Preserve.PIs*/ {
    A[PI] = X(0,0,1, 1);
  }
}

void BIEventMapBuilder::initFull() {
  if (Packed) {
    this->initDocContent<true>();
    this->initDocEnd<true>();
  } else {
    this->initDocContent<false>();
    this->initDocEnd<false>();
  }
}

#undef X

//////////////////////////////////////////////////////////////////////////
// Second Level

inline void BIEventMapBuilder::initCCItems(eventmap::FatElementArray& A,
                                           unsigned I, const unsigned Bits) {
  A[SE] = {I++, Bits};
  A[CH] = {I++, Bits};
  if (Preserve.DTDs)
    A[ER] = {I++, Bits};
  if (!HasThirdLevelCodes(Preserve))
    return;
  
  if (HasCMPI(Preserve)) {
    // Represents n.[(m+3).1]
    unsigned SHIFT = (Packed ? 1 : 8);
    unsigned J = I << SHIFT;
    A[CM] = {J | 0, Bits + SHIFT};
    A[PI] = {J | 1, Bits + SHIFT};
  } else if (Preserve.Comments) {
    A[CM] = {I, Bits};
  } else /*Preserve.PIs*/ {
    A[PI] = {I, Bits};
  }
}

inline void BIEventMapBuilder::initStartTagContent() {
  auto& A = TMap.StartTagContent;
  unsigned Count = 2;                  // {EE, AT}
  Count += Preserve.Prefixes;          // NS
  Count += SelfContained;              // SC
  Count += CountCCItemsBits(Preserve); // {SE, CH, ER, CM, PI}
  const unsigned Bits = Packed ? BISmallLog2[Count] : 8;
  // Do init for primary events.
  unsigned I = 0;
  A[EE] = {I++, Bits};
  A[AT] = {I++, Bits};
  if (Preserve.Prefixes)
    A[NS] = {I++, Bits};
  if (SelfContained)
    A[SC] = {I++, Bits};
  initCCItems(A, I, Bits);
}

inline void BIEventMapBuilder::initElementContent() {
  auto& A = TMap.ElementContent;
  const unsigned Bits = Packed
    ? BISmallLog2[CountCCItemsBits(Preserve)] : 8;
  // No second-level events other than CCItems.
  initCCItems(A, 0, Bits);
}

void BIEventMapBuilder::initSecondLevel() {
  this->initStartTagContent();
  this->initElementContent();
}
