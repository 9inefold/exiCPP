//===- exi/Grammar/BIEventMap.cpp -----------------------------------===//
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

#include <exi/Grammar/BIEventMap.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/WithColor.hpp>
#include <exi/Encode/BodyEncoder.hpp>

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

#if EXI_DEBUG || EXI_ENABLE_DUMP
static auto FormatTerm(SimpleEventTerm Term) {
  return left_justify(get_event_fullname(Term), 8);
}

//////////////////////////////////////////////////////////////////////////
// dumpNonPacked

static void DumpCodeNonPacked(FullEventCode C) {
  unsigned Bytes = C.Bits / 8;
  if (Bytes == 0)
    return;
  u32 Curr = C.Data >> ((Bytes - 1) * 8);
  errs() << (Curr & 0xFF);
  for (unsigned Ix = 2; Ix <= Bytes; ++Ix) {
    Curr = C.Data >> ((Bytes - Ix) * 8);
    errs() << '.' << (Curr & 0xFF);
  }
}
static void DumpCodeNonPacked(u32 First, SecondLevelEventCode C) {
  unsigned Bytes = C.Bits / 8;
  errs() << First;
  if (Bytes == 0)
    return;
  for (unsigned Ix = 1; Ix <= Bytes; ++Ix) {
    u32 Curr = C.Data >> ((Bytes - Ix) * 8);
    errs() << '.' << (Curr & 0xFF);
  }
}

static void DumpArrayNonPacked(auto& A,
                               std::initializer_list<SimpleEventTerm> TermList) {
  for (auto Term : TermList) {
    FullEventCode C = A[Term];
    if (C.Bits == 0)
      continue;
    errs() << "  " << FormatTerm(Term);
    DumpCodeNonPacked(C);
    errs() << '\n';
  }
}
static void DumpArrayNonPacked(u32 FirstLevel, auto& A,
                               std::initializer_list<SimpleEventTerm> TermList) {
  for (auto Term : TermList) {
    SecondLevelEventCode C = A[Term];
    if (C.Bits == 0)
      continue;
    errs() << "  " << FormatTerm(Term);
    DumpCodeNonPacked(FirstLevel, C);
    errs() << '\n';
  }
}

inline void BIEventMap::dumpNonPacked(const ExiOptions& Opts) const {
  [[maybe_unused]] ExiOptions::PreserveOpts Preserve = Opts.Preserve;
  errs() << "Document:\n  " << FormatTerm(SD) << 0 << "\n";

  errs() << "\nDocContent:\n";
  DumpArrayNonPacked(DocContent, {SE, DT, CM, PI});

  errs() << "\nDocEnd:\n";
  DumpArrayNonPacked(DocEnd, {ED, CM, PI});

  errs() << "\nStartTagContent:\n";
  DumpArrayNonPacked(0, StartTagContent, {EE, AT, NS, SC, SE, CH, ER, CM, PI});

  errs() << "\nElementContent:\n";
  errs() << "  " << FormatTerm(EE) << 0 << '\n';
  DumpArrayNonPacked(1, ElementContent, {SE, CH, ER, CM, PI});
}

//////////////////////////////////////////////////////////////////////////
// dumpPacked

// TODO: Implement

inline void BIEventMap::dumpPacked(const ExiOptions& Opts) const {
  [[maybe_unused]] ExiOptions::PreserveOpts Preserve = Opts.Preserve;
  errs() << "Document:\n  " << FormatTerm(SD) << 0 << "\n\n";
  WithColor(errs(), raw_ostream::BRIGHT_RED)
    << "TODO: Implement packed dump!\n\n";
  //exi_todo("implement packed dump");
}

//////////////////////////////////////////////////////////////////////////
// dump

EXI_DUMP_METHOD void BIEventMap::dump(BodyEncoder* BE) const {
  if (BE == nullptr) {
    LOG_ERROR("Dumping Encoder cannot be null!");
    return;
  }
  if (BE->streamKind() == StreamBase::SK_Bit)
    return this->dumpPacked(BE->getOptions());
  else
    return this->dumpNonPacked(BE->getOptions());
}

EXI_DUMP_METHOD void BIEventMap::dump(const ExiOptions& Opts) const {
  if (Opts.Alignment == AlignKind::None) {
    LOG_ERROR("AlignKind cannot be None!");
    return;
  }
  if (Opts.Alignment == AlignKind::BitPacked)
    return this->dumpPacked(Opts);
  else
    return this->dumpNonPacked(Opts);
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
  
  template <bool _3, bool _2, bool _1, unsigned V>
  static constexpr u32 Data =
    (DataRaw<_1, _2, _3>) >> ((3 - V) * Off);

  template <bool _3, bool _2, bool _1, unsigned V>
  static constexpr FullEventCode v = {
    .Data = Data<_3, _2, _1, V>,
    .Bits = (V * Off)
  };
};

template <bool Packed,
  bool _3, bool _2, bool _1, unsigned V>
inline constexpr FullEventCode ecgen_v
  = ECGen<Packed>::template v<_3, _2, _1, V>;
} // namespace `anonymous`

#define X(...) ecgen_v<PACKED, __VA_ARGS__>

template <bool PACKED>
inline void BIEventMapBuilder::initDocContent() {
  eventmap::DocContentArray& A = TMap.DocContent;
  if (Preserve.DTDs) {
    A[DT] = X(0,0,1, 1);
    A[SE].Bits = (PACKED ? 1 : 8);
  }
  if (!hasThirdLevelCodes())
    return;

  if (Preserve.DTDs) {
    A[DT]   = X(0,0,1, 2);
    if (hasCMPI()) {
      A[CM] = X(0,1,1, 3);
      A[PI] = X(1,1,1, 3);
    } else if (Preserve.Comments) {
      A[CM] = X(0,1,1, 2);
    } else /*Preserve.PIs*/ {
      A[PI] = X(0,1,1, 2);
    }
  } else {
    if (hasCMPI()) {
      A[CM] = X(0,0,1, 2);
      A[PI] = X(0,1,1, 2);
    } else if (Preserve.Comments) {
      A[CM] = X(0,0,1, 1);
    } else /*Preserve.PIs*/ {
      A[PI] = X(0,0,1, 1);
    }
  }
}

template <bool PACKED>
inline void BIEventMapBuilder::initDocEnd() {
  if (!hasThirdLevelCodes())
    return;
  
  eventmap::DocEndArray& A = TMap.DocEnd;
  A[ED].Bits = (PACKED ? 1 : 8);
  if (hasCMPI()) {
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
  if (!hasThirdLevelCodes())
    return;
  
  if (hasCMPI()) {
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
  unsigned Count = 2;          // {EE, AT}
  Count += Preserve.Prefixes;  // NS
  Count += SelfContained;      // SC
  Count += countCCItemsBits(); // {SE, CH, ER, CM, PI}
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
    ? BISmallLog2[countCCItemsBits()] : 8;
  // No second-level events other than CCItems.
  initCCItems(A, 0, Bits);
}

void BIEventMapBuilder::initSecondLevel() {
  this->initStartTagContent();
  this->initElementContent();
}
