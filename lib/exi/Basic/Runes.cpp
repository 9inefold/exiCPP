//===- exi/Basic/Runes.cpp ------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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
/// This file defines an interface for unicode codepoints (runes).
///
//===----------------------------------------------------------------===//

#include <exi/Basic/Runes.hpp>
#include <core/Common/ArrayRef.hpp>
#include <core/Common/SmallVec.hpp>

using namespace exi;

/// TODO: Benchmark this baybee
namespace { constexpr usize kMaxWidthBeforeReserve = 64; }

/// Determines whether reserve should be called. Bases it off the raw bytes,
/// as most strings will probably just be ASCII anyways.
ALWAYS_INLINE static void ReserveData(const RuneDecoder& Decoder,
                                      SmallVecImpl<Rune>& Runes) {
  if (Decoder.sizeInBytes() > kMaxWidthBeforeReserve)
    Runes.reserve_back(Decoder.sizeInBytes());
}

bool exi::decodeRunes(RuneDecoder Decoder,
                      SmallVecImpl<Rune>& Runes) {
  ReserveData(Decoder, Runes);
  bool Result = true;
  while (Decoder) {
    const Rune Decoded = Decoder.decode();
    if EXI_UNLIKELY(Decoded == kInvalidRune)
      Result = false;
    Runes.push_back(Decoded);
  }
  return Result;
}

bool exi::decodeRunesUnchecked(RuneDecoder Decoder,
                               SmallVecImpl<Rune>& Runes) {
  ReserveData(Decoder, Runes);
  while (Decoder) {
    Runes.push_back(
      Decoder.decodeUnchecked());
  }
  return true;
}

bool exi::encodeRunes(ArrayRef<Rune> Runes,
                      SmallVecImpl<char>& Chars) {
  // ...
  RuneBuf Buf;
  auto* const Data = Buf.data();
  bool Result = true;

  Chars.reserve_back(Runes.size());
  for (Rune C : Runes) {
    /// Surrogate pairs are invalid under UTF8.
    if EXI_UNLIKELY(C >= 0xD800 && C <= 0xDFFF) {
      C = kInvalidRune;
      Result = false;
    }
    if EXI_UNLIKELY(!RuneEncoder::Encode<true>(C, Buf))
      Result = false;
    Chars.append(Data, Data + Buf.size());
  }
  
  return Result;
}

bool exi::encodeRunesUnchecked(ArrayRef<Rune> Runes,
                               SmallVecImpl<char>& Chars) {
  RuneBuf Buf;
  auto* const Data = Buf.data();

  Chars.reserve_back(Runes.size());
  for (Rune C : Runes) {
    RuneEncoder::Encode(C, Buf);
    Chars.append(Data, Data + Buf.size());
  }

  return true;
}
