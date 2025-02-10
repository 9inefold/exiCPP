//===- Common/APSInt.cpp --------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
/// This file implements the APSInt class, which is a simple class that
/// represents an arbitrary sized integer that knows its signedness.
///
//===----------------------------------------------------------------===//

#include <Common/APSInt.hpp>
// #include <Common/FoldingSet.hpp>
#include <Common/StrRef.hpp>
#include <Common/SmallStr.hpp>
#include <cassert>

using namespace exi;

APSInt::APSInt(StrRef Str) {
  exi_assert(!Str.empty(), "Invalid string length");

  // (Over-)estimate the required number of bits.
  unsigned NumBits = ((Str.size() * 64) / 19) + 2;
  APInt Tmp(NumBits, Str, /*radix=*/10);
  if (Str[0] == '-') {
    unsigned MinBits = Tmp.getSignificantBits();
    if (MinBits < NumBits)
      Tmp = Tmp.trunc(std::max<unsigned>(1, MinBits));
    *this = APSInt(Tmp, /*isUnsigned=*/false);
    return;
  }
  unsigned ActiveBits = Tmp.getActiveBits();
  if (ActiveBits < NumBits)
    Tmp = Tmp.trunc(std::max<unsigned>(1, ActiveBits));
  *this = APSInt(Tmp, /*isUnsigned=*/true);
}

// void APSInt::Profile(FoldingSetNodeID& ID) const {
//   ID.AddInteger((unsigned) (IsUnsigned ? 1 : 0));
//   APInt::Profile(ID);
// }

std::string exi::format_as(const APSInt& APS) {
  SmallStr<40> Str;
  APS.toString(Str, 10, APS.isSigned(), /* formatAsCLiteral = */false);
  return std::string(Str.begin(), Str.end());
}
