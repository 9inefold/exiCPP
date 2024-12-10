//===- Common/StringExtras.hpp --------------------------------------===//
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
/// This file contains some functions that are useful when dealing with strings.
///
//===----------------------------------------------------------------===//

#pragma once

// TODO: Implement
// #include "llvm/ADT/APSInt.h"
#include <Common/ArrayRef.hpp>
#include <Common/Fundamental.hpp>
#include <Common/SmallStr.hpp>
#include <Common/String.hpp>
#include <Common/Twine.hpp>
#include <Common/Vec.hpp>
#include <Support/ErrorHandle.hpp>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>

namespace exi {

class raw_ostream;

/// hexdigit - Return the hexadecimal character for the
/// given number \p X (which should be less than 16).
inline char hexdigit(unsigned X, bool LowerCase = false) {
  exi_assert(X < 16);
  static constexpr const char LUT[] = "0123456789ABCDEF";
  const u8 Offset = LowerCase ? 32 : 0;
  return LUT[X] | Offset;
}

/// Given an array of c-style strings terminated by a null pointer, construct
/// a vector of StringRefs representing the same strings without the terminating
/// null string.
inline Vec<StrRef> toStringRefArray(const char *const *Strings) {
  Vec<StrRef> Result;
  while (*Strings)
    Result.push_back(*Strings++);
  return Result;
}

/// Construct a string ref from a boolean.
inline StrRef toStringRef(bool B) { return StrRef(B ? "true" : "false"); }

/// Construct a string ref from an array ref of unsigned chars.
inline StrRef toStringRef(ArrayRef<u8> Input) {
  return StrRef(reinterpret_cast<const char *>(Input.begin()), Input.size());
}
inline StrRef toStringRef(ArrayRef<char> Input) {
  return StrRef(Input.begin(), Input.size());
}

/// Construct a string ref from an array ref of unsigned chars.
template <class CharT = u8>
inline ArrayRef<CharT> arrayRefFromStringRef(StrRef Input) {
  static_assert(std::is_same<CharT, char>::value ||
                    std::is_same<CharT, unsigned char>::value ||
                    std::is_same<CharT, signed char>::value,
                "Expected byte type");
  return ArrayRef<CharT>(reinterpret_cast<const CharT *>(Input.data()),
                         Input.size());
}

/// Interpret the given character \p C as a hexadecimal digit and return its
/// value.
///
/// If \p C is not a valid hex digit, -1U is returned.
inline unsigned hexDigitValue(char C) {
  /* clang-format off */
  static const i16 LUT[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,  // '0'..'9'
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 'A'..'F'
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 'a'..'f'
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  };
  /* clang-format on */
  return LUT[static_cast<unsigned char>(C)];
}

/// Checks if character \p C is one of the 10 decimal digits.
inline bool isDigit(char C) { return C >= '0' && C <= '9'; }

/// Checks if character \p C is a hexadecimal numeric character.
inline bool isHexDigit(char C) { return hexDigitValue(C) != ~0U; }

/// Checks if character \p C is a lowercase letter as classified by "C" locale.
inline bool isLower(char C) { return 'a' <= C && C <= 'z'; }

/// Checks if character \p C is a uppercase letter as classified by "C" locale.
inline bool isUpper(char C) { return 'A' <= C && C <= 'Z'; }

/// Checks if character \p C is a valid letter as classified by "C" locale.
inline bool isAlpha(char C) { return isLower(C) || isUpper(C); }

/// Checks whether character \p C is either a decimal digit or an uppercase or
/// lowercase letter as classified by "C" locale.
inline bool isAlnum(char C) { return isAlpha(C) || isDigit(C); }

/// Checks whether character \p C is valid ASCII (high bit is zero).
inline bool isASCII(char C) { return static_cast<unsigned char>(C) <= 127; }

/// Checks whether all characters in S are ASCII.
inline bool isASCII(StrRef S) {
  for (char C : S)
    if (EXI_UNLIKELY(!isASCII(C)))
      return false;
  return true;
}

/// Checks whether character \p C is printable.
///
/// Locale-independent version of the C standard library isprint whose results
/// may differ on different platforms.
inline bool isPrint(char C) {
  unsigned char UC = static_cast<unsigned char>(C);
  return (0x20 <= UC) && (UC <= 0x7E);
}

/// Checks whether character \p C is a punctuation character.
///
/// Locale-independent version of the C standard library ispunct. The list of
/// punctuation characters can be found in the documentation of std::ispunct:
/// https://en.cppreference.com/w/cpp/string/byte/ispunct.
inline bool isPunct(char C) {
  static constexpr StringLiteral Punctuations =
      R"(!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~)";
  return Punctuations.contains(C);
}

/// Checks whether character \p C is whitespace in the "C" locale.
///
/// Locale-independent version of the C standard library isspace.
inline bool isSpace(char C) {
  return C == ' ' || C == '\f' || C == '\n' || C == '\r' || C == '\t' ||
         C == '\v';
}

/// Returns the corresponding lowercase character if \p x is uppercase.
inline char toLower(char x) {
  if (isUpper(x))
    return x - 'A' + 'a';
  return x;
}

/// Returns the corresponding uppercase character if \p x is lowercase.
inline char toUpper(char x) {
  if (isLower(x))
    return x - 'a' + 'A';
  return x;
}

inline Str utohexstr(u64 X, bool LowerCase = false,
                             unsigned Width = 0) {
  char Buffer[17];
  char *BufPtr = std::end(Buffer);

  if (X == 0) *--BufPtr = '0';

  for (unsigned i = 0; Width ? (i < Width) : X; ++i) {
    unsigned char Mod = static_cast<unsigned char>(X) & 15;
    *--BufPtr = hexdigit(Mod, LowerCase);
    X >>= 4;
  }

  return Str(BufPtr, std::end(Buffer));
}

/// Convert buffer \p Input to its hexadecimal representation.
/// The returned string is double the size of \p Input.
inline void toHex(ArrayRef<u8> Input, bool LowerCase,
                  SmallVecImpl<char> &Output) {
  const usize Length = Input.size();
  Output.resize_for_overwrite(Length * 2);

  for (usize i = 0; i < Length; i++) {
    const u8 c = Input[i];
    Output[i * 2    ] = hexdigit(c >> 4, LowerCase);
    Output[i * 2 + 1] = hexdigit(c & 15, LowerCase);
  }
}

inline Str toHex(ArrayRef<u8> Input, bool LowerCase = false) {
  SmallStr<16> Output;
  toHex(Input, LowerCase, Output);
  return Str(Output);
}

inline Str toHex(StrRef Input, bool LowerCase = false) {
  return toHex(arrayRefFromStringRef(Input), LowerCase);
}

/// Store the binary representation of the two provided values, \p MSB and
/// \p LSB, that make up the nibbles of a hexadecimal digit. If \p MSB or \p LSB
/// do not correspond to proper nibbles of a hexadecimal digit, this method
/// returns false. Otherwise, returns true.
inline bool tryGetHexFromNibbles(char MSB, char LSB, u8 &Hex) {
  unsigned U1 = hexDigitValue(MSB);
  unsigned U2 = hexDigitValue(LSB);
  if (U1 == ~0U || U2 == ~0U)
    return false;

  Hex = static_cast<u8>((U1 << 4) | U2);
  return true;
}

/// Return the binary representation of the two provided values, \p MSB and
/// \p LSB, that make up the nibbles of a hexadecimal digit.
inline u8 hexFromNibbles(char MSB, char LSB) {
  u8 Hex = 0;
  bool GotHex = tryGetHexFromNibbles(MSB, LSB, Hex);
  (void)GotHex;
  exi_assert(GotHex, "MSB and/or LSB do not correspond to hex digits");
  return Hex;
}

/// Convert hexadecimal string \p Input to its binary representation and store
/// the result in \p Output. Returns true if the binary representation could be
/// converted from the hexadecimal string. Returns false if \p Input contains
/// non-hexadecimal digits. The output string is half the size of \p Input.
inline bool tryGetFromHex(StrRef Input, Str &Output) {
  if (Input.empty())
    return true;

  // If the input string is not properly aligned on 2 nibbles we pad out the
  // front with a 0 prefix; e.g. `ABC` -> `0ABC`.
  Output.resize((Input.size() + 1) / 2);
  char *OutputPtr = const_cast<char *>(Output.data());
  if (Input.size() % 2 == 1) {
    u8 Hex = 0;
    if (!tryGetHexFromNibbles('0', Input.front(), Hex))
      return false;
    *OutputPtr++ = Hex;
    Input = Input.drop_front();
  }

  // Convert the nibble pairs (e.g. `9C`) into bytes (0x9C).
  // With the padding above we know the input is aligned and the output expects
  // exactly half as many bytes as nibbles in the input.
  usize InputSize = Input.size();
  exi_assert(InputSize % 2 == 0);
  const char *InputPtr = Input.data();
  for (usize OutputIndex = 0; OutputIndex < InputSize / 2; ++OutputIndex) {
    u8 Hex = 0;
    if (!tryGetHexFromNibbles(InputPtr[OutputIndex * 2 + 0], // MSB
                              InputPtr[OutputIndex * 2 + 1], // LSB
                              Hex))
      return false;
    OutputPtr[OutputIndex] = Hex;
  }
  return true;
}

/// Convert hexadecimal string \p Input to its binary representation.
/// The return string is half the size of \p Input.
inline Str fromHex(StrRef Input) {
  Str Hex;
  bool GotHex = tryGetFromHex(Input, Hex);
  (void)GotHex;
  exi_assert(GotHex, "Input contains non hex digits");
  return Hex;
}

/// Convert the string \p S to an integer of the specified type using
/// the radix \p Base.  If \p Base is 0, auto-detects the radix.
/// Returns true if the number was successfully converted, false otherwise.
template <typename N> bool to_integer(StrRef S, N &Num, unsigned Base = 0) {
  return !S.getAsInteger(Base, Num);
}

namespace detail {
template <typename N>
inline bool to_float(const Twine &T, N &Num, N (*StrTo)(const char *, char **)) {
  SmallStr<32> Storage;
  StrRef S = T.toNullTerminatedStringRef(Storage);
  char *End;
  N Temp = StrTo(S.data(), &End);
  if (*End != '\0')
    return false;
  Num = Temp;
  return true;
}
}

inline bool to_float(const Twine &T, float &Num) {
  return detail::to_float(T, Num, &strtof);
}

inline bool to_float(const Twine &T, double &Num) {
  return detail::to_float(T, Num, &strtod);
}

inline bool to_float(const Twine &T, long double &Num) {
  return detail::to_float(T, Num, &strtold);
}

inline Str utostr(u64 X, bool isNeg = false) {
  char Buffer[21];
  char *BufPtr = std::end(Buffer);

  if (X == 0) *--BufPtr = '0';  // Handle special case...

  while (X) {
    *--BufPtr = '0' + char(X % 10);
    X /= 10;
  }

  if (isNeg) *--BufPtr = '-';   // Add negative sign...
  return Str(BufPtr, std::end(Buffer));
}

inline Str itostr(i64 X) {
  if (X < 0)
    return utostr(static_cast<u64>(1) + ~static_cast<u64>(X), true);
  else
    return utostr(static_cast<u64>(X));
}

#if 0
inline Str toString(const APInt &I, unsigned Radix, bool Signed,
                            bool formatAsCLiteral = false,
                            bool UpperCase = true,
                            bool InsertSeparators = false) {
  SmallStr<40> S;
  I.toString(S, Radix, Signed, formatAsCLiteral, UpperCase, InsertSeparators);
  return Str(S);
}

inline Str toString(const APSInt &I, unsigned Radix) {
  return toString(I, Radix, I.isSigned());
}
#endif

/// StrInStrNoCase - Portable version of strcasestr.  Locates the first
/// occurrence of string 's1' in string 's2', ignoring case.  Returns
/// the offset of s2 in s1 or npos if s2 cannot be found.
StrRef::size_type StrInStrNoCase(StrRef s1, StrRef s2);

/// getToken - This function extracts one token from source, ignoring any
/// leading characters that appear in the Delimiters string, and ending the
/// token at any of the characters that appear in the Delimiters string.  If
/// there are no tokens in the source string, an empty string is returned.
/// The function returns a pair containing the extracted token and the
/// remaining tail string.
std::pair<StrRef, StrRef> getToken(StrRef Source,
                                         StrRef Delimiters = " \t\n\v\f\r");

/// SplitString - Split up the specified string according to the specified
/// delimiters, appending the result fragments to the output list.
void SplitString(StrRef Source,
                 SmallVecImpl<StrRef> &OutFragments,
                 StrRef Delimiters = " \t\n\v\f\r");

/// Returns the English suffix for an ordinal integer (-st, -nd, -rd, -th).
inline StrRef getOrdinalSuffix(unsigned Val) {
  // It is critically important that we do this perfectly for
  // user-written sequences with over 100 elements.
  switch (Val % 100) {
  case 11:
  case 12:
  case 13:
    return "th";
  default:
    switch (Val % 10) {
      case 1: return "st";
      case 2: return "nd";
      case 3: return "rd";
      default: return "th";
    }
  }
}

/// Print each character of the specified string, escaping it if it is not
/// printable or if it is an escape char.
void printEscapedString(StrRef Name, raw_ostream &Out);

/// Print each character of the specified string, escaping HTML special
/// characters.
void printHTMLEscaped(StrRef String, raw_ostream &Out);

/// printLowerCase - Print each character as lowercase if it is uppercase.
void printLowerCase(StrRef String, raw_ostream &Out);

/// Converts a string from camel-case to snake-case by replacing all uppercase
/// letters with '_' followed by the letter in lowercase, except if the
/// uppercase letter is the first character of the string.
Str convertToSnakeFromCamelCase(StrRef input);

/// Converts a string from snake-case to camel-case by replacing all occurrences
/// of '_' followed by a lowercase letter with the letter in uppercase.
/// Optionally allow capitalization of the first letter (if it is a lowercase
/// letter)
Str convertToCamelFromSnakeCase(StrRef input,
                                        bool capitalizeFirst = false);

namespace detail {

template <typename IteratorT>
inline Str join_impl(IteratorT Begin, IteratorT End,
                             StrRef Separator, std::input_iterator_tag) {
  Str S;
  if (Begin == End)
    return S;

  S += (*Begin);
  while (++Begin != End) {
    S += Separator;
    S += (*Begin);
  }
  return S;
}

template <typename IteratorT>
inline Str join_impl(IteratorT Begin, IteratorT End,
                             StrRef Separator, std::forward_iterator_tag) {
  Str S;
  if (Begin == End)
    return S;

  usize Len = (std::distance(Begin, End) - 1) * Separator.size();
  for (IteratorT I = Begin; I != End; ++I)
    Len += StrRef(*I).size();
  S.reserve(Len);
  usize PrevCapacity = S.capacity();
  (void)PrevCapacity;
  S += (*Begin);
  while (++Begin != End) {
    S += Separator;
    S += (*Begin);
  }
  exi_assert(PrevCapacity == S.capacity(), "String grew during building");
  return S;
}

template <typename Sep>
inline void join_items_impl(Str &Result, Sep Separator) {}

template <typename Sep, typename Arg>
inline void join_items_impl(Str &Result, Sep Separator,
                            const Arg &Item) {
  Result += Item;
}

template <typename Sep, typename Arg1, typename... Args>
inline void join_items_impl(Str &Result, Sep Separator, const Arg1 &A1,
                            Args &&... Items) {
  Result += A1;
  Result += Separator;
  join_items_impl(Result, Separator, std::forward<Args>(Items)...);
}

inline usize join_one_item_size(char) { return 1; }
inline usize join_one_item_size(const char *S) { return S ? ::strlen(S) : 0; }

template <typename T> inline usize join_one_item_size(const T &S) {
  return S.size();
}

template <typename... Args> inline usize join_items_size(Args &&...Items) {
  return (0 + ... + join_one_item_size(std::forward<Args>(Items)));
}

} // end namespace detail

/// Joins the strings in the range [Begin, End), adding Separator between
/// the elements.
template <typename IteratorT>
inline Str join(IteratorT Begin, IteratorT End, StrRef Separator) {
  using tag = typename std::iterator_traits<IteratorT>::iterator_category;
  return detail::join_impl(Begin, End, Separator, tag());
}

/// Joins the strings in the range [R.begin(), R.end()), adding Separator
/// between the elements.
template <typename Range>
inline Str join(Range &&R, StrRef Separator) {
  return join(R.begin(), R.end(), Separator);
}

/// Joins the strings in the parameter pack \p Items, adding \p Separator
/// between the elements.  All arguments must be implicitly convertible to
/// Str, or there should be an overload of Str::operator+=()
/// that accepts the argument explicitly.
template <typename Sep, typename... Args>
inline Str join_items(Sep Separator, Args &&... Items) {
  Str Result;
  if (sizeof...(Items) == 0)
    return Result;

  usize NS = detail::join_one_item_size(Separator);
  usize NI = detail::join_items_size(std::forward<Args>(Items)...);
  Result.reserve(NI + (sizeof...(Items) - 1) * NS + 1);
  detail::join_items_impl(Result, Separator, std::forward<Args>(Items)...);
  return Result;
}

/// A helper class to return the specified delimiter string after the first
/// invocation of operator StrRef().  Used to generate a comma-separated
/// list from a loop like so:
///
/// \code
///   ListSeparator LS;
///   for (auto &I : C)
///     OS << LS << I.getName();
/// \end
class ListSeparator {
  bool First = true;
  StrRef Separator;

public:
  ListSeparator(StrRef Separator = ", ") : Separator(Separator) {}
  operator StrRef() {
    if (First) {
      First = false;
      return {};
    }
    return Separator;
  }
};

/// A forward iterator over partitions of string over a separator.
class SplittingIterator
    : public iterator_facade_base<SplittingIterator, std::forward_iterator_tag,
                                  StrRef> {
  char SeparatorStorage;
  StrRef Current;
  StrRef Next;
  StrRef Separator;

public:
  SplittingIterator(StrRef S, StrRef Separator)
      : Next(S), Separator(Separator) {
    ++*this;
  }

  SplittingIterator(StrRef S, char Separator)
      : SeparatorStorage(Separator), Next(S),
        Separator(&SeparatorStorage, 1) {
    ++*this;
  }

  SplittingIterator(const SplittingIterator &R)
      : SeparatorStorage(R.SeparatorStorage), Current(R.Current), Next(R.Next),
        Separator(R.Separator) {
    if (R.Separator.data() == &R.SeparatorStorage)
      Separator = StrRef(&SeparatorStorage, 1);
  }

  SplittingIterator &operator=(const SplittingIterator &R) {
    if (this == &R)
      return *this;

    SeparatorStorage = R.SeparatorStorage;
    Current = R.Current;
    Next = R.Next;
    Separator = R.Separator;
    if (R.Separator.data() == &R.SeparatorStorage)
      Separator = StrRef(&SeparatorStorage, 1);
    return *this;
  }

  bool operator==(const SplittingIterator &R) const {
    exi_assert(Separator == R.Separator);
    return Current.data() == R.Current.data();
  }

  const StrRef &operator*() const { return Current; }

  StrRef &operator*() { return Current; }

  SplittingIterator &operator++() {
    std::tie(Current, Next) = Next.split(Separator);
    return *this;
  }
};

/// Split the specified string over a separator and return a range-compatible
/// iterable over its partitions.  Used to permit conveniently iterating
/// over separated strings like so:
///
/// \code
///   for (StrRef x : exi::split("foo,bar,baz", ","))
///     ...;
/// \end
///
/// Note that the passed string must remain valid throuhgout lifetime
/// of the iterators.
inline iterator_range<SplittingIterator> split(StrRef S, StrRef Separator) {
  return {SplittingIterator(S, Separator),
          SplittingIterator(StrRef(), Separator)};
}

inline iterator_range<SplittingIterator> split(StrRef S, char Separator) {
  return {SplittingIterator(S, Separator),
          SplittingIterator(StrRef(), Separator)};
}

} // namespace exi
