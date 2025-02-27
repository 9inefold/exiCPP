//===- Support/Format.hpp --------------------------------------------===//
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
/// This file defines an interface for formatting into streams.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/ArrayRef.hpp>
#include <Common/StrRef.hpp>
#include <Common/Option.hpp>
#include <Common/SmallStr.hpp>
#include <fmt/base.h>

namespace exi {

class raw_ostream;

template <class Ctx, usize NumArgs, usize Desc>
using fmt_arg_store = fmt::detail::format_arg_store<Ctx, NumArgs, 0, Desc>;

class IFormatObject {
protected:
  StrRef Fmt;
  fmt::format_args VArgs;

  template <class Ctx, usize NumArgs, usize Desc>
  IFormatObject(
    StrRef Fmt, const fmt_arg_store<Ctx, NumArgs, Desc>& Sto) :
   Fmt(Fmt), VArgs(Sto) {
  }

public:
  usize print(char* Buffer, usize BufferSize) const;
  void format(raw_ostream& OS) const;
  void toVector(SmallVecImpl<char>& Vec) const;

  String str() const;
  operator String() const { return str(); }

  template <unsigned N>
  SmallStr<N> sstr() const {
    SmallStr<N> Out;
    this->toVector(Out);
    return Out;
  }
  
  template <unsigned N>
  operator SmallStr<N>() const { return sstr<N>(); }
};

template <typename...Args>
class FormatObject final : public IFormatObject {
  fmt::vargs<const Args&...> Sto;
public:
  FormatObject(fmt::string_view Str, const Args&...args) :
   IFormatObject(
    StrRef(Str.data(), Str.size()),
    this->Sto
   ),
   Sto{{args...}} {
  }
};

template <typename...Args>
EXI_INLINE auto format(
  fmt::format_string<const Args&...> Fmt,
  const Args&...args EXI_LIFETIMEBOUND)
 -> FormatObject<Args...> {
  return FormatObject<Args...>(Fmt, args...);
}

//////////////////////////////////////////////////////////////////////////
// Single Objects - Everything below under the LLVM license

// This is a helper class for left_justify, right_justify, and center_justify.
class FormattedString {
public:
  enum Justification { JustifyNone, JustifyLeft, JustifyRight, JustifyCenter };
  FormattedString(StrRef S, unsigned W, Justification J)
      : Str(S), Width(W), Justify(J) {}

private:
  StrRef Str;
  unsigned Width;
  Justification Justify;
  friend class raw_ostream;
};

/// left_justify - append spaces after string so total output is
/// \p Width characters.  If \p Str is larger that \p Width, full string
/// is written with no padding.
inline FormattedString left_justify(StrRef Str, unsigned Width) {
  return FormattedString(Str, Width, FormattedString::JustifyLeft);
}

/// right_justify - add spaces before string so total output is
/// \p Width characters.  If \p Str is larger that \p Width, full string
/// is written with no padding.
inline FormattedString right_justify(StrRef Str, unsigned Width) {
  return FormattedString(Str, Width, FormattedString::JustifyRight);
}

/// center_justify - add spaces before and after string so total output is
/// \p Width characters.  If \p Str is larger that \p Width, full string
/// is written with no padding.
inline FormattedString center_justify(StrRef Str, unsigned Width) {
  return FormattedString(Str, Width, FormattedString::JustifyCenter);
}

/// This is a helper class used for format_hex() and format_decimal().
class FormattedNumber {
  u64 HexValue;
  int64_t DecValue;
  unsigned Width;
  bool Hex;
  bool Upper;
  bool HexPrefix;
  friend class raw_ostream;

public:
  FormattedNumber(u64 HV, int64_t DV, unsigned W, bool H, bool U,
                  bool Prefix)
      : HexValue(HV), DecValue(DV), Width(W), Hex(H), Upper(U),
        HexPrefix(Prefix) {}
};

/// format_hex - Output \p N as a fixed width hexadecimal. If number will not
/// fit in width, full number is still printed.  Examples:
///   OS << format_hex(255, 4)              => 0xff
///   OS << format_hex(255, 4, true)        => 0xFF
///   OS << format_hex(255, 6)              => 0x00ff
///   OS << format_hex(255, 2)              => 0xff
inline FormattedNumber format_hex(u64 N, unsigned Width,
                                  bool Upper = false) {
  assert(Width <= 18 && "hex width must be <= 18");
  return FormattedNumber(N, 0, Width, true, Upper, true);
}

/// format_hex_no_prefix - Output \p N as a fixed width hexadecimal. Does not
/// prepend '0x' to the outputted string.  If number will not fit in width,
/// full number is still printed.  Examples:
///   OS << format_hex_no_prefix(255, 2)              => ff
///   OS << format_hex_no_prefix(255, 2, true)        => FF
///   OS << format_hex_no_prefix(255, 4)              => 00ff
///   OS << format_hex_no_prefix(255, 1)              => ff
inline FormattedNumber format_hex_no_prefix(u64 N, unsigned Width,
                                            bool Upper = false) {
  assert(Width <= 16 && "hex width must be <= 16");
  return FormattedNumber(N, 0, Width, true, Upper, false);
}

/// format_decimal - Output \p N as a right justified, fixed-width decimal. If
/// number will not fit in width, full number is still printed.  Examples:
///   OS << format_decimal(0, 5)     => "    0"
///   OS << format_decimal(255, 5)   => "  255"
///   OS << format_decimal(-1, 3)    => " -1"
///   OS << format_decimal(12345, 3) => "12345"
inline FormattedNumber format_decimal(int64_t N, unsigned Width) {
  return FormattedNumber(0, N, Width, false, false, false);
}

class FormattedBytes {
  ArrayRef<u8> Bytes;

  // If not std::nullopt, display offsets for each line relative to starting
  // value.
  Option<u64> FirstByteOffset;
  u32 IndentLevel;    // Number of characters to indent each line.
  u32 NumPerLine;     // Number of bytes to show per line.
  u8  ByteGroupSize;  // How many hex bytes are grouped without spaces
  bool Upper;         // Show offset and hex bytes as upper case.
  bool ASCII;         // Show the ASCII bytes for the hex bytes to the right.
  friend class raw_ostream;

public:
  FormattedBytes(ArrayRef<u8> B, u32 IL, Option<u64> O,
                 u32 NPL, u8 BGS, bool U, bool A)
      : Bytes(B), FirstByteOffset(O), IndentLevel(IL), NumPerLine(NPL),
        ByteGroupSize(BGS), Upper(U), ASCII(A) {

    if (ByteGroupSize > NumPerLine)
      ByteGroupSize = NumPerLine;
  }
};

inline FormattedBytes
format_bytes(ArrayRef<u8> Bytes,
             Option<u64> FirstByteOffset = nullopt,
             u32 NumPerLine = 16, u8 ByteGroupSize = 4,
             u32 IndentLevel = 0, bool Upper = false) {
  return FormattedBytes(Bytes, IndentLevel, FirstByteOffset, NumPerLine,
                        ByteGroupSize, Upper, false);
}

inline FormattedBytes
format_bytes_with_ascii(ArrayRef<u8> Bytes,
                        Option<u64> FirstByteOffset = nullopt,
                        u32 NumPerLine = 16, u8 ByteGroupSize = 4,
                        u32 IndentLevel = 0, bool Upper = false) {
  return FormattedBytes(Bytes, IndentLevel, FirstByteOffset, NumPerLine,
                        ByteGroupSize, Upper, true);
}

} // namespace exi
