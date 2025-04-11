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

#if EXI_INVARIANTS
inline constexpr bool kCheckRunes = true;
#else
inline constexpr bool kCheckRunes = false;
#endif

class RuneEncoder;
class RuneDecoder;

/// A simple 4-byte buffer to hold encoded runes. Can be freely passed by value.
class RuneBuf {
  friend class RuneEncoder;
  friend class RuneDecoder;
  char Data[4] = {};
  u32 Length = 0;

  ALWAYS_INLINE constexpr RuneBuf& putUnchecked(u8 Val) {
    Data[Length++] = static_cast<char>(Val);
    return *this;
  }

public:
  constexpr RuneBuf() = default;

  constexpr RuneBuf& put(u8 Val) {
    if EXI_UNLIKELY(Length == 4)
      return *this;
    return putUnchecked(Val);
  }

  ALWAYS_INLINE constexpr RuneBuf& reset() {
    Length = 0;
    return *this;
  }

  ALWAYS_INLINE constexpr usize size() const { return Length; }
  ALWAYS_INLINE constexpr const char* data() const { return Data; }

  constexpr StrRef str() const { return {Data, Length}; }
  EXI_INLINE constexpr operator StrRef() const { return this->str(); }
};

/// A simple UTF8 -> UTF32 decoder. It can be run in a checked or unchecked
/// mode, allowing for more efficient decoding. The latter requires external
/// validation before running the decoder.
///
/// The checked decoder does not follow Unicode's current error replacement
/// guidelines, instead replacing entire error sequences with the invalid rune.
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

  constexpr RuneDecoder(const u8* Begin, usize Size) :
   Data(Begin), Length(Size) {
    exi_invariant(Begin || Size == 0, "Invalid decoder input");
    // TODO: Consider if this should be checked in release.
  }

  ALWAYS_INLINE illegal_constexpr RuneDecoder(const char* Begin, usize Size) :
   RuneDecoder(FOLD_CXPR(reinterpret_cast<const u8*>(Begin)), Size) {
  }

#if defined(__cpp_char8_t)
  ALWAYS_INLINE illegal_constexpr
    RuneDecoder(const char8_t* Begin, usize Size) :
   RuneDecoder(FOLD_CXPR(reinterpret_cast<const u8*>(Begin)), Size) {
  }
#endif

  illegal_constexpr ALWAYS_INLINE RuneDecoder(StrRef Str) :
   RuneDecoder(Str.data(), Str.size()) {
  }

  illegal_constexpr ALWAYS_INLINE RuneDecoder(ArrayRef<char> Str) :
   RuneDecoder(Str.data(), Str.size()) {
  }

  ALWAYS_INLINE constexpr RuneDecoder(ArrayRef<u8> Data) :
   RuneDecoder(Data.data(), Data.size()) {
  }

  ALWAYS_INLINE constexpr usize sizeInBytes() const {
    return Length;
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

  template <usize N>
  ALWAYS_INLINE constexpr usize checkTrail() const {
    static_assert(N >= 1 && N < 4);
    return (Data[N] & 0b1100'0000) == 0b1000'0000;
  }

  template <usize N>
  ALWAYS_INLINE constexpr usize checkTrailSeq() const {
    if constexpr (N == 1)
      return checkTrail<N>();
    else
      return checkTrail<N>() && checkTrailSeq<N - 1>();
  }

  /// Decodes 1-byte utf8 codepoints.
  ALWAYS_INLINE constexpr Rune decode1() const {
    exi_invariant(Length >= 1);
    return Data[0];
  }

  /// Decodes 2-byte utf8 codepoints.
  template <bool Check>
  ALWAYS_INLINE constexpr Rune decode2() const {
    exi_invariant(Length >= 2);
    if (Check) {
      if (not checkTrailSeq<1>())
        return U'�';
    }
    return ((Data[0] & kCode2) << 6) |
            (Data[1] & kTrail);
  }

  /// Decodes 3-byte utf8 codepoints.
  template <bool Check>
  ALWAYS_INLINE constexpr Rune decode3() const {
    exi_invariant(Length >= 3);
    if (Check) {
      if (not checkTrailSeq<2>())
        return U'�';
    }
    return ((Data[0] & kCode3) << 12) |
           ((Data[1] & kTrail) << 6)  |
            (Data[2] & kTrail);
  }

  /// Decodes 4-byte utf8 codepoints.
  template <bool Check>
  ALWAYS_INLINE constexpr Rune decode4() const {
    exi_invariant(Length >= 4);
    if (Check) {
      if (not checkTrailSeq<3>())
        return U'�';
    }
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

  /// Decodes N-byte utf8 codepoints.
  template <bool Check>
  constexpr Rune decodeImpl(usize N) const {
    if EXI_LIKELY(N == 1)
      return decode1();
    else if (N == 2)
      return decode2<Check>();
    else if (N == 3)
      return decode3<Check>();
    else
      return decode4<Check>();
  }

public:
  ALWAYS_INLINE constexpr usize currentLen() const {
    if EXI_UNLIKELY(Length == 0)
      return 0;
    return UnitLen(*Data);
  }

  template <bool Checked = true>
  EXI_INLINE constexpr std::pair<Rune, usize> peek() const {
    const usize NBytes = UnitLen(*Data);
    if constexpr (Checked) {
      if EXI_UNLIKELY(NBytes > Length)
        return {U'�', Length};
    }
    return {decodeImpl<Checked>(NBytes), NBytes};
  }

  /// Decodes UTF8 to unicode codepoints without validity checking.
  /// Only use when you know the data is definitely valid.
  constexpr Rune decodeUnchecked() {
    auto [Out, NBytes] = peek</*Checked=*/false>();
    advance(NBytes);
    return Out;
  }

  /// Decodes UTF8 to unicode codepoints with some basic validity checking.
  constexpr Rune decode() {
    if EXI_UNLIKELY(!Data || Length == 0)
      return U'�';

    auto [Out, NBytes] = peek</*Checked=*/true>();
    if EXI_UNLIKELY(NBytes > Length) {
      Data += Length;
      Length = 0;
      return Out;
    }

    // TODO: Check for overlong sequences?
    // Might not be necessary with simdutf integration...
    advance(NBytes);
    return EXI_LIKELY(Out <= kMaxVal) ? Out : U'�';
  }

  /// Checks if the decoder reached the end of the data.
  constexpr explicit operator bool() const {
    return EXI_LIKELY(Data) && (Length > 0);
  }

  class sentinel {};
  class iterator;

  constexpr iterator begin() const;
  constexpr sentinel end() const { return sentinel{}; }
};

class RuneDecoder::iterator {
  RuneDecoder Data;
public:
  constexpr iterator(RuneDecoder Decoder) : Data(Decoder) {}
  constexpr Rune operator*() const { return Data.peek<kCheckRunes>().first; }
  constexpr RuneDecoder* operator->() { return &Data; }
  constexpr const RuneDecoder* operator->() const { return &Data; }

  constexpr iterator& operator++() {
    Data.advance(Data.currentLen());
    return *this;
  }
  constexpr iterator operator++(int) {
    const iterator Out = *this;
    Data.advance(Data.currentLen());
    return Out;
  }

  constexpr bool operator==(const iterator& RHS) const {
    return (Data.Data == RHS->Data) && (Data.Length == RHS->Length);
  }
  constexpr bool operator==(sentinel) const {
    return Data.Length == 0;
  }
};

ALWAYS_INLINE constexpr RuneDecoder::iterator
 RuneDecoder::begin() const { return iterator(*this); }

/// A simple UTF32 -> UTF8 decoder. It does not handle checking for things such
/// as surrogate pairs directly, but it will when `exi::encodeRunes` is used.
class RuneEncoder {
  enum : u32 {
    kASCII = 0x7f,
    kCode2 = 0x7ff,
    kCode3 = 0xffff,
    kCode4 = 0x10ffff,
  };

  template <bool Ret, typename TrueType = bool>
  using RetType = std::conditional_t<Ret, TrueType, void>;

public:
  template <bool ReturnCode = false>
  EXI_INLINE static RetType<ReturnCode>
   Encode(const Rune C, RuneBuf& Out) noexcept {
    if (C <= kASCII) {
      Out.reset()
        .putUnchecked(C);
    } else if (C <= kCode2) {
      Out.reset()
        .putUnchecked(0xc0 | (C >> 6))
        .putUnchecked(0x80 | (C & 0x3f));
    } else if (C <= kCode3) {
      Out.reset()
        .putUnchecked(0xe0 |  (C >> 12))
        .putUnchecked(0x80 | ((C >> 6) & 0x3f))
        .putUnchecked(0x80 |  (C & 0x3f));
    } else if (C <= kCode4) {
      Out.reset()
        .putUnchecked(0xf0 |  (C >> 18))
        .putUnchecked(0x80 | ((C >> 12) & 0x3f))
        .putUnchecked(0x80 | ((C >> 6) & 0x3f))
        .putUnchecked(0x80 |  (C & 0x3f));
    } else {
      RuneEncoder::Encode(U'�', Out);
      return RetType<ReturnCode>(false);
    }

    return RetType<ReturnCode>(true);
  }

  static RuneBuf Encode(const Rune C) noexcept {
    RuneBuf Out;
    RuneEncoder::Encode<false>(C, Out);
    return Out;
  }
};

/// Safely decodes codepoints from the input and inserts them into `Runes`.
/// @returns Whether the decoding was successful.
bool decodeRunes(RuneDecoder Decoder, SmallVecImpl<Rune>& Runes);

/// Decodes codepoints from the input and inserts them into `Runes`.
/// Does no validity checking, the return is just for consistency.
/// Only use when you know the data is definitely valid.
bool decodeRunesUnchecked(RuneDecoder Decoder, SmallVecImpl<Rune>& Runes);

/// Safely encodes UTF8 from the input and inserts them into `Chars`.
/// @returns Whether the decoding was successful.
bool encodeRunes(ArrayRef<Rune> Runes, SmallVecImpl<char>& Chars);

/// Encodes UTF8 from the input and inserts them into `Chars`.
/// Does no validity checking, the return is just for consistency.
/// Only use when you know the data is definitely valid.
bool encodeRunesUnchecked(ArrayRef<Rune> Runes, SmallVecImpl<char>& Chars);

} // namespace exi
