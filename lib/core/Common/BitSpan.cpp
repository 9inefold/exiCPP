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

using namespace exi;

BitSpan<BitSpanWordType>::BitSpan(const BitVector& BV EXI_LIFETIMEBOUND)
 : BitSpan(BV.Bits.data(), BV.Size) {}

BitVector BitSpan<BitSpanWordType>::to_bitvector() const {
  return BitVector(*this);
}

//////////////////////////////////////////////////////////////////////////
// BitVector

BitVector::BitVector(BitSpan<BitWord> BS, bool Clear)
 : Bits(BS.Bits, BS.Bits + BS.Words), Size(BS.Size) {
  if (Clear)
    this->clear_unused_bits();
}
