//===- exi/Basic/Runes.hpp ------------------------------------------===//
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

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Support/ErrorHandle.hpp>

namespace exi {

template <typename T> class SmallVecImpl;

using Rune = u32;
inline constexpr Rune kInvalidRune = U'�'; 

class RuneDecoder {
  enum : u32 {
    kASCII = 0b0111'1111,
    kCode2 = 0b0001'1111,
    kCode3 = 0b0000'1111,
    kCode4 = 0b0000'0111,
    kTrail = 0b0011'1111,

    kMask2 = 0b111'00000,
    kMask3 = 0b1111'0000,
    kMask4 = 0b11111'000,

    kHead2 = 0b110'00000,
    kHead3 = 0b1110'0000,
    kHead4 = 0b11110'000,

    kMaxVal = 0x10ffff,
  };

  const u8* Data = nullptr;
  usize Length = 0;

public:
  constexpr RuneDecoder() = default;

  RuneDecoder(const u8* Begin, usize Size) : Data(Begin), Length(Size) {
    exi_invariant(Begin || Size == 0, "Invalid decoder input");
  }

  ALWAYS_INLINE RuneDecoder(const char* Begin, usize Size) :
   RuneDecoder(reinterpret_cast<const u8*>(Begin), Size) {
  }

#if defined(__cpp_char8_t)
  ALWAYS_INLINE RuneDecoder(const char8_t* Begin, usize Size) :
   RuneDecoder(reinterpret_cast<const u8*>(Begin), Size) {
  }
#endif

  ALWAYS_INLINE RuneDecoder(StrRef Str) :
   RuneDecoder(Str.data(), Str.size()) {
  }

  ALWAYS_INLINE RuneDecoder(ArrayRef<char> Str) :
   RuneDecoder(Str.data(), Str.size()) {
  }

  ALWAYS_INLINE RuneDecoder(ArrayRef<u8> Data) :
   RuneDecoder(Data.data(), Data.size()) {
  }

private:
  ALWAYS_INLINE static constexpr usize UnitLen(u8 Head) {
    if ((Head & kMask4) == kHead4)
      return 4;
    else if ((Head & kMask3) == kHead3)
      return 3;
    else if ((Head & kMask2) == kHead2)
      return 2;
    else
      return 1;
  }

  ALWAYS_INLINE constexpr Rune decode1() const {
    exi_invariant(Length >= 1);
    return Data[0];
  }

  ALWAYS_INLINE constexpr Rune decode2() const {
    exi_invariant(Length >= 2);
    return ((Data[0] & kCode2) << 6) |
            (Data[1] & kTrail);
  }

  ALWAYS_INLINE constexpr Rune decode3() const {
    exi_invariant(Length >= 3);
    return ((Data[0] & kCode3) << 12) |
           ((Data[1] & kTrail) << 6)  |
            (Data[2] & kTrail);
  }

  ALWAYS_INLINE constexpr Rune decode4() const {
    exi_invariant(Length >= 4);
    return ((Data[0] & kCode4) << 18) |
           ((Data[1] & kTrail) << 12) |
           ((Data[2] & kTrail) << 6)  |
            (Data[3] & kTrail);
  }

  ALWAYS_INLINE constexpr void advance(usize N) {
    exi_invariant(Length >= N && N <= 4);
    Data += N;
    Length -= N;
  }

  constexpr Rune decodeImpl(usize N) {
    if EXI_LIKELY(N == 1)
      return decode1();
    else if (N == 2)
      return decode2();
    else if (N == 3)
      return decode3();
    else
      return decode4();
  }

public:
  /// Decodes UTF8 to unicode codepoints without validity checking.
  /// Only use when you know the data is definitely valid.
  constexpr Rune decodeUnchecked() {
    const usize NBytes = UnitLen(*Data);
    const Rune Out = decodeImpl(NBytes);
    advance(NBytes);
    return Out;
  }

  /// Decodes UTF8 to unicode codepoints with some basic validity checking.
  constexpr Rune decode() {
    if EXI_UNLIKELY(!Data || Length == 0)
      return U'�';

    const usize NBytes = UnitLen(*Data);
    if EXI_UNLIKELY(NBytes > Length) {
      Data += Length;
      Length = 0;
      return U'�';
    }

    // TODO: Check for overlong sequences?
    // Might not be necessary with simdutf integration...
    const Rune Out = decodeImpl(NBytes);
    advance(NBytes);
    return EXI_LIKELY(Out <= kMaxVal) ? Out : U'�';
  }

  /// Checks if the decoder reached the end of the data.
  constexpr explicit operator bool() const {
    return EXI_LIKELY(Data) && (Length > 0);
  } 
};

bool decodeRunes(RuneDecoder Decoder, SmallVecImpl<Rune>& Runes);
bool decodeRunesUnchecked(RuneDecoder Decoder, SmallVecImpl<Rune>& Runes);

} // namespace exi
