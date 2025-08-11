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

//////////////////////////////////////////////////////////////////////////
// Full

namespace {
template <bool Packed> struct ECGen {
  static constexpr unsigned Off = Packed ? 1 : 8;

  template <bool _3, bool _2, bool _1>
  static constexpr u32 Data =
    (u32(_3) << (2 * Off)) | (u32(_2) << (1 * Off)) | u32(_1);
  
  template <bool _3, bool _2, bool _1, unsigned V>
  static constexpr FullEventCode v = {
    .Data = Data<_3, _2, _1>,
    .Bits = (V * Off)
  };
};
} // namespace `anonymous`

#define X(...) G::template v<__VA_ARGS__>

template <bool PACKED>
inline void BIEventMapBuilder::initDocContent() {
  using G = ECGen<PACKED>;
  auto& A = TMap.DocContent;
  if (Preserve.DTDs) {
    A[T::IdxDocContent(DT)] = X(0,0,1, 1);
    A[T::IdxDocContent(SE)].Bits = (1 * G::Off);
  }
  if (!hasThirdLevelCodes())
    return;

  if (Preserve.DTDs) {
    A[T::IdxDocContent(DT)]   = X(0,0,1, 2);
    if (hasCMPI()) {
      A[T::IdxDocContent(CM)] = X(0,1,1, 3);
      A[T::IdxDocContent(PI)] = X(1,1,1, 3);
    } else if (Preserve.Comments) {
      A[T::IdxDocContent(CM)] = X(0,1,1, 2);
    } else /*Preserve.PIs*/ {
      A[T::IdxDocContent(PI)] = X(0,1,1, 2);
    }
  } else {
    if (hasCMPI()) {
      A[T::IdxDocContent(CM)] = X(0,0,1, 2);
      A[T::IdxDocContent(PI)] = X(0,1,1, 2);
    } else if (Preserve.Comments) {
      A[T::IdxDocContent(CM)] = X(0,0,1, 1);
    } else /*Preserve.PIs*/ {
      A[T::IdxDocContent(PI)] = X(0,0,1, 1);
    }
  }
}

template <bool PACKED>
inline void BIEventMapBuilder::initDocEnd() {
  using G = ECGen<PACKED>;
  if (!hasThirdLevelCodes())
    return;
  
  auto& A = TMap.DocEnd;
  A[T::IdxDocEnd(ED)].Bits = (1 * G::Off);
  if (hasCMPI()) {
    A[T::IdxDocEnd(CM)] = X(0,0,1, 2);
    A[T::IdxDocEnd(PI)] = X(0,1,1, 2);
  } else if (Preserve.Comments) {
    A[T::IdxDocEnd(CM)] = X(0,0,1, 1);
  } else /*Preserve.PIs*/ {
    A[T::IdxDocEnd(PI)] = X(0,0,1, 1);
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

inline void BIEventMapBuilder::initCCItems(BIEventMap::FatElementArray& A,
                                           unsigned I, const unsigned Bits) {
  A[SE] = {I++, Bits};
  A[CH] = {I++, Bits};
  if (Preserve.DTDs)
    A[ER] = {I++, Bits};
  if (!hasThirdLevelCodes())
    return;
  
  if (hasCMPI()) {
    // Represents n.[(m+3).1]
    unsigned J = (1 << Bits) | I;
    A[CM] = {I, Bits + (Packed ? 1 : 8)};
    A[PI] = {J, Bits + (Packed ? 1 : 8)};
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
  // No second-level events other than CCItems.
  initCCItems(A, 0, BISmallLog2[countCCItemsBits()]);
}

void BIEventMapBuilder::initSecondLevel() {
  this->initStartTagContent();
  this->initElementContent();
}
