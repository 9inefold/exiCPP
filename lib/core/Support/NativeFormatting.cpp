//===- Support/NativeFormatting.cpp ---------------------------------===//
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

#include <Support/NativeFormatting.hpp>
#include <Common/ArrayRef.hpp>
#include <Common/SmallStr.hpp>
#include <Common/StringExtras.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/raw_ostream.hpp>
#include <cmath>

using namespace exi;

template <typename T, usize N>
static int format_to_buffer(T Value, char (&Buffer)[N]) {
  char *EndPtr = std::end(Buffer);
  char *CurPtr = EndPtr;

  do {
    *--CurPtr = '0' + char(Value % 10);
    Value /= 10;
  } while (Value);
  return EndPtr - CurPtr;
}

static void writeWithCommas(raw_ostream &S, ArrayRef<char> Buffer) {
  exi_assert(!Buffer.empty());

  ArrayRef<char> ThisGroup;
  int InitialDigits = ((Buffer.size() - 1) % 3) + 1;
  ThisGroup = Buffer.take_front(InitialDigits);
  S.write(ThisGroup.data(), ThisGroup.size());

  Buffer = Buffer.drop_front(InitialDigits);
  exi_assert(Buffer.size() % 3 == 0);
  while (!Buffer.empty()) {
    S << ',';
    ThisGroup = Buffer.take_front(3);
    S.write(ThisGroup.data(), 3);
    Buffer = Buffer.drop_front(3);
  }
}

template <typename T>
static void write_unsigned_impl(raw_ostream &S, T N, usize MinDigits,
                                IntStyle Style, bool IsNegative) {
  static_assert(std::is_unsigned_v<T>, "Value is not unsigned!");

  char NumberBuffer[128];
  usize Len = format_to_buffer(N, NumberBuffer);

  if (IsNegative)
    S << '-';

  if (Len < MinDigits && Style != IntStyle::Number) {
    for (usize I = Len; I < MinDigits; ++I)
      S << '0';
  }

  if (Style == IntStyle::Number) {
    writeWithCommas(S, ArrayRef<char>(std::end(NumberBuffer) - Len, Len));
  } else {
    S.write(std::end(NumberBuffer) - Len, Len);
  }
}

template <typename T>
static void write_unsigned(raw_ostream &S, T N, usize MinDigits,
                           IntStyle Style, bool IsNegative = false) {
  // Output using 32-bit div/mod if possible.
  if (N == static_cast<u32>(N))
    write_unsigned_impl(S, static_cast<u32>(N), MinDigits, Style,
                        IsNegative);
  else
    write_unsigned_impl(S, N, MinDigits, Style, IsNegative);
}

template <typename T>
static void write_signed(raw_ostream &S, T N, usize MinDigits,
                         IntStyle Style) {
  static_assert(std::is_signed_v<T>, "Value is not signed!");

  using UnsignedT = std::make_unsigned_t<T>;

  if (N >= 0) {
    write_unsigned(S, static_cast<UnsignedT>(N), MinDigits, Style);
    return;
  }

  UnsignedT UN = -(UnsignedT)N;
  write_unsigned(S, UN, MinDigits, Style, true);
}

void exi::write_integer(raw_ostream &S, unsigned int N, usize MinDigits,
                         IntStyle Style) {
  write_unsigned(S, N, MinDigits, Style);
}

void exi::write_integer(raw_ostream &S, int N, usize MinDigits,
                         IntStyle Style) {
  write_signed(S, N, MinDigits, Style);
}

void exi::write_integer(raw_ostream &S, unsigned long N, usize MinDigits,
                         IntStyle Style) {
  write_unsigned(S, N, MinDigits, Style);
}

void exi::write_integer(raw_ostream &S, long N, usize MinDigits,
                         IntStyle Style) {
  write_signed(S, N, MinDigits, Style);
}

void exi::write_integer(raw_ostream &S, unsigned long long N, usize MinDigits,
                         IntStyle Style) {
  write_unsigned(S, N, MinDigits, Style);
}

void exi::write_integer(raw_ostream &S, long long N, usize MinDigits,
                         IntStyle Style) {
  write_signed(S, N, MinDigits, Style);
}

void exi::write_hex(raw_ostream &S, u64 N, HexPrintStyle Style,
                    Option<usize> Width) {
  constexpr usize kMaxWidth = 128u;

  usize W = std::min(kMaxWidth, Width.value_or(0u));

  unsigned Nibbles = (std::bit_width(N) + 3) / 4;
  bool Prefix = (Style == HexPrintStyle::PrefixLower ||
                 Style == HexPrintStyle::PrefixUpper);
  bool Upper =
      (Style == HexPrintStyle::Upper || Style == HexPrintStyle::PrefixUpper);
  unsigned PrefixChars = Prefix ? 2 : 0;
  unsigned NumChars =
      std::max(static_cast<unsigned>(W), std::max(1u, Nibbles) + PrefixChars);

  char NumberBuffer[kMaxWidth];
  ::memset(NumberBuffer, '0', std::size(NumberBuffer));
  if (Prefix)
    NumberBuffer[1] = 'x';
  char *EndPtr = NumberBuffer + NumChars;
  char *CurPtr = EndPtr;
  while (N) {
    unsigned char x = static_cast<unsigned char>(N) % 16;
    *--CurPtr = hexdigit(x, !Upper);
    N /= 16;
  }

  S.write(NumberBuffer, NumChars);
}

void exi::write_double(raw_ostream &S, double N, FloatStyle Style,
                       Option<usize> Precision) {
  usize Prec = Precision.value_or(getDefaultPrecision(Style));

  if (std::isnan(N)) {
    S << "nan";
    return;
  } else if (std::isinf(N)) {
    S << (std::signbit(N) ? "-INF" : "INF");
    return;
  }

  char Letter;
  if (Style == FloatStyle::Exponent)
    Letter = 'e';
  else if (Style == FloatStyle::ExponentUpper)
    Letter = 'E';
  else
    Letter = 'f';

  SmallStr<8> Spec;
  exi::raw_svector_ostream Out(Spec);
  Out << "{:." << Prec << Letter << '}';

  if (Style == FloatStyle::Percent)
    N *= 100.0;

  StaticFmtBuffer<32> Buf;
  Buf.format(fmt::runtime(Spec.str()), N);
  S << Buf;
  if (Style == FloatStyle::Percent)
    S << '%';
}

bool exi::isPrefixedHexStyle(HexPrintStyle S) {
  return (S == HexPrintStyle::PrefixLower || S == HexPrintStyle::PrefixUpper);
}

usize exi::getDefaultPrecision(FloatStyle Style) {
  switch (Style) {
  case FloatStyle::Exponent:
  case FloatStyle::ExponentUpper:
    return 6; // Number of decimal places.
  case FloatStyle::Fixed:
  case FloatStyle::Percent:
    return 2; // Number of decimal places.
  }
  exi_unreachable("Unknown FloatStyle enum");
}
