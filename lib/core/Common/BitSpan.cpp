//===- Common/BitSpan.cpp --------------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements the BitSpan class.
///
//===----------------------------------------------------------------===//

#include <Common/BitSpan.hpp>
#include <Common/BitVector.hpp>
#include <Common/SmallStr.hpp>

using namespace exi;

BitSpan<BitSpanWordType>::BitSpan(const BitVector& BV EXI_LIFETIMEBOUND)
 : BitSpan(BV.Bits.data(), BV.Size) {}

BitVector BitSpan<BitSpanWordType>::to_bitvector() const {
  return BitVector(*this);
}

String BitSpan<BitSpanWordType>::to_string(char Zero, char One) const {
  String Out(Size, Zero);
  for (unsigned Ix = 0; Ix != Size; ++Ix)
    if (this->test(Ix))
      Out[Size - 1 - Ix] = One;
  return Out;
}

void BitSpan<BitSpanWordType>::write(SmallVecImpl<char>& Out,
                                        char Zero, char One) const {
  const unsigned Off = Out.size();
  Out.reserve_back(Size);
  Out.append(Zero, Size);

  for (unsigned Ix = 0; Ix != Size; ++Ix)
    if (this->test(Ix))
      Out[Size - 1 - Ix + Off] = One;
}

raw_ostream& exi::operator<<(raw_ostream& OS, const BitSpan<>& B) {
  SmallStr<128> Out;
  Out.reserve(B.size());
  B.write(Out);
  return OS << Out;
}

//////////////////////////////////////////////////////////////////////////
// BitVector

BitVector::BitVector(BitSpan<BitWord> BS, bool Clear)
 : Bits(BS.Bits, BS.Bits + BS.Words), Size(BS.Size) {
  if (Clear)
    this->clear_unused_bits();
}
