//===- Common/StrRef.cpp --------------------------------------------===//
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

#include <Common/StrRef.hpp>
#if EXI_HAS_AP_SCALARS
# include "llvm/ADT/APFloat.h"
# include "llvm/ADT/APInt.h"
#endif
// #include "llvm/ADT/Hashing.h"
#include <Common/StringExtras.hpp>
#include <Common/edit_distance.hpp>
// #include "llvm/Support/Error.h"
#include <bitset>

using namespace exi;

// MSVC emits references to this into the translation units which reference it.
#ifndef _MSC_VER
constexpr usize StrRef::npos;
#endif

// strncasecmp() is not available on non-POSIX systems, so define an
// alternative function here.
static int AsciiStrncasecmp(const char *LHS, const char *RHS, usize Length) {
  for (usize I = 0; I < Length; ++I) {
    unsigned char LHC = toLower(LHS[I]);
    unsigned char RHC = toLower(RHS[I]);
    if (LHC != RHC)
      return LHC < RHC ? -1 : 1;
  }
  return 0;
}

int StrRef::compare_insensitive(StrRef RHS) const {
  if (int Res =
          AsciiStrncasecmp(data(), RHS.data(), std::min(size(), RHS.size())))
    return Res;
  if (size() == RHS.size())
    return 0;
  return size() < RHS.size() ? -1 : 1;
}

bool StrRef::starts_with_insensitive(StrRef Prefix) const {
  return size() >= Prefix.size() &&
         AsciiStrncasecmp(data(), Prefix.data(), Prefix.size()) == 0;
}

bool StrRef::ends_with_insensitive(StrRef Suffix) const {
  return size() >= Suffix.size() &&
         AsciiStrncasecmp(end() - Suffix.size(), Suffix.data(),
                           Suffix.size()) == 0;
}

usize StrRef::find_insensitive(char C, usize From) const {
  char L = toLower(C);
  return find_if([L](char D) { return toLower(D) == L; }, From);
}

/// compare_numeric - Compare strings, handle embedded numbers.
int StrRef::compare_numeric(StrRef RHS) const {
  for (usize I = 0, E = std::min(size(), RHS.size()); I != E; ++I) {
    // Check for sequences of digits.
    if (isDigit(data()[I]) && isDigit(RHS.data()[I])) {
      // The longer sequence of numbers is considered larger.
      // This doesn't really handle prefixed zeros well.
      usize J;
      for (J = I + 1; J != E + 1; ++J) {
        bool ld = J < size() && isDigit(data()[J]);
        bool rd = J < RHS.size() && isDigit(RHS.data()[J]);
        if (ld != rd)
          return rd ? -1 : 1;
        if (!rd)
          break;
      }
      // The two number sequences have the same length (J-I), just memcmp them.
      if (int Res = compareMemory(data() + I, RHS.data() + I, J - I))
        return Res < 0 ? -1 : 1;
      // Identical number sequences, continue search after the numbers.
      I = J - 1;
      continue;
    }
    if (data()[I] != RHS.data()[I])
      return (unsigned char)data()[I] < (unsigned char)RHS.data()[I] ? -1 : 1;
  }
  if (size() == RHS.size())
    return 0;
  return size() < RHS.size() ? -1 : 1;
}

// Compute the edit distance between the two given strings.
unsigned StrRef::edit_distance(exi::StrRef Other,
                                  bool AllowReplacements,
                                  unsigned MaxEditDistance) const {
  return exi::ComputeEditDistance(ArrayRef(data(), size()),
                                   ArrayRef(Other.data(), Other.size()),
                                   AllowReplacements, MaxEditDistance);
}

unsigned exi::StrRef::edit_distance_insensitive(
    StrRef Other, bool AllowReplacements, unsigned MaxEditDistance) const {
  return exi::ComputeMappedEditDistance(
      ArrayRef(data(), size()), ArrayRef(Other.data(), Other.size()),
      exi::toLower, AllowReplacements, MaxEditDistance);
}

//===----------------------------------------------------------------------===//
// String Operations
//===----------------------------------------------------------------------===//

namespace {


template <typename ItTy, typename FuncTy,
          typename ReferenceTy =
              decltype(std::declval<FuncTy>()(*std::declval<ItTy>()))>
class int_mapped_iterator
    : public iterator_adaptor_base<
          int_mapped_iterator<ItTy, FuncTy>, ItTy,
          typename std::iterator_traits<ItTy>::iterator_category,
          std::remove_reference_t<ReferenceTy>,
          typename std::iterator_traits<ItTy>::difference_type,
          std::remove_reference_t<ReferenceTy> *, ReferenceTy> {
public:
  int_mapped_iterator() = default;
  int_mapped_iterator(ItTy U, FuncTy F)
    : int_mapped_iterator::iterator_adaptor_base(std::move(U)), F(std::move(F)) {}

  ItTy getCurrent() { return this->I; }

  const FuncTy &getFunction() const { return F; }

  ReferenceTy operator*() const { return F(*this->I); }

private:
  typename H::CallbackType<FuncTy>::value_type F{};
};

template <class ItTy, class FuncTy>
inline int_mapped_iterator<ItTy, FuncTy> int_map_iterator(ItTy I, FuncTy F) {
  return int_mapped_iterator<ItTy, FuncTy>(std::move(I), std::move(F));
}

} // namespace `anonymous`

Str StrRef::lower() const {
  return Str(int_map_iterator(begin(), toLower),
                     int_map_iterator(end(), toLower));
}

Str StrRef::upper() const {
  return Str(int_map_iterator(begin(), toUpper),
                     int_map_iterator(end(), toUpper));
}

//===----------------------------------------------------------------------===//
// String Searching
//===----------------------------------------------------------------------===//


/// find - Search for the first string \arg S in the string.
///
/// \return - The index of the first occurrence of \arg S, or npos if not
/// found.
usize StrRef::find(StrRef S, usize From) const {
  if (From > size())
    return npos;

  const char *Start = data() + From;
  usize Size = size() - From;

  const char *Needle = S.data();
  usize N = S.size();
  if (N == 0)
    return From;
  if (Size < N)
    return npos;
  if (N == 1) {
    const char *Ptr = (const char *)::memchr(Start, Needle[0], Size);
    return Ptr == nullptr ? npos : Ptr - data();
  }

  const char *Stop = Start + (Size - N + 1);

  if (N == 2) {
    // Provide a fast path for newline finding (CRLF case) in InclusionRewriter.
    // Not the most optimized strategy, but getting memcmp inlined should be
    // good enough.
    do {
      if (std::memcmp(Start, Needle, 2) == 0)
        return Start - data();
      ++Start;
    } while (Start < Stop);
    return npos;
  }

  // For short haystacks or unsupported needles fall back to the naive algorithm
  if (Size < 16 || N > 255) {
    do {
      if (std::memcmp(Start, Needle, N) == 0)
        return Start - data();
      ++Start;
    } while (Start < Stop);
    return npos;
  }

  // Build the bad char heuristic table, with uint8_t to reduce cache thrashing.
  uint8_t BadCharSkip[256];
  std::memset(BadCharSkip, N, 256);
  for (unsigned i = 0; i != N-1; ++i)
    BadCharSkip[(uint8_t)S[i]] = N-1-i;

  do {
    uint8_t Last = Start[N - 1];
    if (EXI_UNLIKELY(Last == (uint8_t)Needle[N - 1]))
      if (std::memcmp(Start, Needle, N - 1) == 0)
        return Start - data();

    // Otherwise skip the appropriate number of bytes.
    Start += BadCharSkip[Last];
  } while (Start < Stop);

  return npos;
}

usize StrRef::find_insensitive(StrRef S, usize From) const {
  StrRef This = substr(From);
  while (This.size() >= S.size()) {
    if (This.starts_with_insensitive(S))
      return From;
    This = This.drop_front();
    ++From;
  }
  return npos;
}

usize StrRef::rfind_insensitive(char C, usize From) const {
  From = std::min(From, size());
  usize i = From;
  while (i != 0) {
    --i;
    if (toLower(data()[i]) == toLower(C))
      return i;
  }
  return npos;
}

/// rfind - Search for the last string \arg S in the string.
///
/// \return - The index of the last occurrence of \arg S, or npos if not
/// found.
usize StrRef::rfind(StrRef S) const {
  return std::string_view(*this).rfind(S);
}

usize StrRef::rfind_insensitive(StrRef S) const {
  usize N = S.size();
  if (N > size())
    return npos;
  for (usize i = size() - N + 1, e = 0; i != e;) {
    --i;
    if (substr(i, N).equals_insensitive(S))
      return i;
  }
  return npos;
}

/// find_first_of - Find the first character in the string that is in \arg
/// Chars, or npos if not found.
///
/// Note: O(size() + Chars.size())
StrRef::size_type StrRef::find_first_of(StrRef Chars,
                                              usize From) const {
  std::bitset<1 << CHAR_BIT> CharBits;
  for (char C : Chars)
    CharBits.set((unsigned char)C);

  for (size_type i = std::min(From, size()), e = size(); i != e; ++i)
    if (CharBits.test((unsigned char)data()[i]))
      return i;
  return npos;
}

/// find_first_not_of - Find the first character in the string that is not
/// \arg C or npos if not found.
StrRef::size_type StrRef::find_first_not_of(char C, usize From) const {
  return std::string_view(*this).find_first_not_of(C, From);
}

/// find_first_not_of - Find the first character in the string that is not
/// in the string \arg Chars, or npos if not found.
///
/// Note: O(size() + Chars.size())
StrRef::size_type StrRef::find_first_not_of(StrRef Chars,
                                                  usize From) const {
  std::bitset<1 << CHAR_BIT> CharBits;
  for (char C : Chars)
    CharBits.set((unsigned char)C);

  for (size_type i = std::min(From, size()), e = size(); i != e; ++i)
    if (!CharBits.test((unsigned char)data()[i]))
      return i;
  return npos;
}

/// find_last_of - Find the last character in the string that is in \arg C,
/// or npos if not found.
///
/// Note: O(size() + Chars.size())
StrRef::size_type StrRef::find_last_of(StrRef Chars,
                                             usize From) const {
  std::bitset<1 << CHAR_BIT> CharBits;
  for (char C : Chars)
    CharBits.set((unsigned char)C);

  for (size_type i = std::min(From, size()) - 1, e = -1; i != e; --i)
    if (CharBits.test((unsigned char)data()[i]))
      return i;
  return npos;
}

/// find_last_not_of - Find the last character in the string that is not
/// \arg C, or npos if not found.
StrRef::size_type StrRef::find_last_not_of(char C, usize From) const {
  for (size_type i = std::min(From, size()) - 1, e = -1; i != e; --i)
    if (data()[i] != C)
      return i;
  return npos;
}

/// find_last_not_of - Find the last character in the string that is not in
/// \arg Chars, or npos if not found.
///
/// Note: O(size() + Chars.size())
StrRef::size_type StrRef::find_last_not_of(StrRef Chars,
                                                 usize From) const {
  std::bitset<1 << CHAR_BIT> CharBits;
  for (char C : Chars)
    CharBits.set((unsigned char)C);

  for (size_type i = std::min(From, size()) - 1, e = -1; i != e; --i)
    if (!CharBits.test((unsigned char)data()[i]))
      return i;
  return npos;
}

void StrRef::split(SmallVecImpl<StrRef> &A,
                      StrRef Separator, int MaxSplit,
                      bool KeepEmpty) const {
  StrRef S = *this;

  // Count down from MaxSplit. When MaxSplit is -1, this will just split
  // "forever". This doesn't support splitting more than 2^31 times
  // intentionally; if we ever want that we can make MaxSplit a 64-bit integer
  // but that seems unlikely to be useful.
  while (MaxSplit-- != 0) {
    usize Idx = S.find(Separator);
    if (Idx == npos)
      break;

    // Push this split.
    if (KeepEmpty || Idx > 0)
      A.push_back(S.slice(0, Idx));

    // Jump forward.
    S = S.substr(Idx + Separator.size());
  }

  // Push the tail.
  if (KeepEmpty || !S.empty())
    A.push_back(S);
}

void StrRef::split(SmallVecImpl<StrRef> &A, char Separator,
                      int MaxSplit, bool KeepEmpty) const {
  StrRef S = *this;

  // Count down from MaxSplit. When MaxSplit is -1, this will just split
  // "forever". This doesn't support splitting more than 2^31 times
  // intentionally; if we ever want that we can make MaxSplit a 64-bit integer
  // but that seems unlikely to be useful.
  while (MaxSplit-- != 0) {
    usize Idx = S.find(Separator);
    if (Idx == npos)
      break;

    // Push this split.
    if (KeepEmpty || Idx > 0)
      A.push_back(S.slice(0, Idx));

    // Jump forward.
    S = S.substr(Idx + 1);
  }

  // Push the tail.
  if (KeepEmpty || !S.empty())
    A.push_back(S);
}

//===----------------------------------------------------------------------===//
// Helpful Algorithms
//===----------------------------------------------------------------------===//

/// count - Return the number of non-overlapped occurrences of \arg S in
/// the string.
usize StrRef::count(StrRef S) const {
  usize Count = 0;
  usize Pos = 0;
  usize N = S.size();
  // TODO: For an empty `S` we return 0 for legacy reasons. Consider changing
  //       this to `Length + 1` which is more in-line with the function
  //       description.
  if (!N)
    return 0;
  while ((Pos = find(S, Pos)) != npos) {
    ++Count;
    Pos += N;
  }
  return Count;
}

static unsigned GetAutoSenseRadix(StrRef &S) {
  if (S.empty())
    return 10;

  if (S.consume_front_insensitive("0x"))
    return 16;

  if (S.consume_front_insensitive("0b"))
    return 2;

  if (S.consume_front("0o"))
    return 8;

  if (S[0] == '0' && S.size() > 1 && isDigit(S[1])) {
    S = S.substr(1);
    return 8;
  }

  return 10;
}

bool exi::consumeUnsignedInteger(StrRef &S, unsigned Radix,
                                  unsigned long long &Result) {
  // Autosense radix if not specified.
  if (Radix == 0)
    Radix = GetAutoSenseRadix(S);

  // Empty strings (after the radix autosense) are invalid.
  if (S.empty()) return true;

  // Parse all the bytes of the string given this radix.  Watch for overflow.
  StrRef Str2 = S;
  Result = 0;
  while (!Str2.empty()) {
    unsigned CharVal;
    if (Str2[0] >= '0' && Str2[0] <= '9')
      CharVal = Str2[0] - '0';
    else if (Str2[0] >= 'a' && Str2[0] <= 'z')
      CharVal = Str2[0] - 'a' + 10;
    else if (Str2[0] >= 'A' && Str2[0] <= 'Z')
      CharVal = Str2[0] - 'A' + 10;
    else
      break;

    // If the parsed value is larger than the integer radix, we cannot
    // consume any more characters.
    if (CharVal >= Radix)
      break;

    // Add in this character.
    unsigned long long PrevResult = Result;
    Result = Result * Radix + CharVal;

    // Check for overflow by shifting back and seeing if bits were lost.
    if (Result / Radix < PrevResult)
      return true;

    Str2 = Str2.substr(1);
  }

  // We consider the operation a failure if no characters were consumed
  // successfully.
  if (S.size() == Str2.size())
    return true;

  S = Str2;
  return false;
}

bool exi::consumeSignedInteger(StrRef &S, unsigned Radix,
                                long long &Result) {
  unsigned long long ULLVal;

  // Handle positive strings first.
  if (!S.starts_with("-")) {
    if (consumeUnsignedInteger(S, Radix, ULLVal) ||
        // Check for value so large it overflows a signed value.
        (long long)ULLVal < 0)
      return true;
    Result = ULLVal;
    return false;
  }

  // Get the positive part of the value.
  StrRef Str2 = S.drop_front(1);
  if (consumeUnsignedInteger(Str2, Radix, ULLVal) ||
      // Reject values so large they'd overflow as negative signed, but allow
      // "-0".  This negates the unsigned so that the negative isn't undefined
      // on signed overflow.
      (long long)-ULLVal > 0)
    return true;

  S = Str2;
  Result = -ULLVal;
  return false;
}

/// GetAsUnsignedInteger - Workhorse method that converts a integer character
/// sequence of radix up to 36 to an unsigned long long value.
bool exi::getAsUnsignedInteger(StrRef S, unsigned Radix,
                                unsigned long long &Result) {
  if (consumeUnsignedInteger(S, Radix, Result))
    return true;

  // For getAsUnsignedInteger, we require the whole string to be consumed or
  // else we consider it a failure.
  return !S.empty();
}

bool exi::getAsSignedInteger(StrRef S, unsigned Radix,
                              long long &Result) {
  if (consumeSignedInteger(S, Radix, Result))
    return true;

  // For getAsSignedInteger, we require the whole string to be consumed or else
  // we consider it a failure.
  return !S.empty();
}

#if EXI_HAS_AP_SCALARS

bool StrRef::consumeInteger(unsigned Radix, APInt &Result) {
  StrRef S = *this;

  // Autosense radix if not specified.
  if (Radix == 0)
    Radix = GetAutoSenseRadix(S);

  exi_assert(Radix > 1 && Radix <= 36);

  // Empty strings (after the radix autosense) are invalid.
  if (S.empty()) return true;

  // Skip leading zeroes.  This can be a significant improvement if
  // it means we don't need > 64 bits.
  S = S.ltrim('0');

  // If it was nothing but zeroes....
  if (S.empty()) {
    Result = APInt(64, 0);
    *this = S;
    return false;
  }

  // (Over-)estimate the required number of bits.
  unsigned Log2Radix = 0;
  while ((1U << Log2Radix) < Radix) Log2Radix++;
  bool IsPowerOf2Radix = ((1U << Log2Radix) == Radix);

  unsigned BitWidth = Log2Radix * S.size();
  if (BitWidth < Result.getBitWidth())
    BitWidth = Result.getBitWidth(); // don't shrink the result
  else if (BitWidth > Result.getBitWidth())
    Result = Result.zext(BitWidth);

  APInt RadixAP, CharAP; // unused unless !IsPowerOf2Radix
  if (!IsPowerOf2Radix) {
    // These must have the same bit-width as Result.
    RadixAP = APInt(BitWidth, Radix);
    CharAP = APInt(BitWidth, 0);
  }

  // Parse all the bytes of the string given this radix.
  Result = 0;
  while (!S.empty()) {
    unsigned CharVal;
    if (S[0] >= '0' && S[0] <= '9')
      CharVal = S[0]-'0';
    else if (S[0] >= 'a' && S[0] <= 'z')
      CharVal = S[0]-'a'+10;
    else if (S[0] >= 'A' && S[0] <= 'Z')
      CharVal = S[0]-'A'+10;
    else
      break;

    // If the parsed value is larger than the integer radix, the string is
    // invalid.
    if (CharVal >= Radix)
      break;

    // Add in this character.
    if (IsPowerOf2Radix) {
      Result <<= Log2Radix;
      Result |= CharVal;
    } else {
      Result *= RadixAP;
      CharAP = CharVal;
      Result += CharAP;
    }

    S = S.substr(1);
  }

  // We consider the operation a failure if no characters were consumed
  // successfully.
  if (size() == S.size())
    return true;

  *this = S;
  return false;
}

bool StrRef::getAsInteger(unsigned Radix, APInt &Result) const {
  StrRef S = *this;
  if (S.consumeInteger(Radix, Result))
    return true;

  // For getAsInteger, we require the whole string to be consumed or else we
  // consider it a failure.
  return !S.empty();
}

bool StrRef::getAsDouble(double &Result, bool AllowInexact) const {
  APFloat F(0.0);
  auto StatusOrErr = F.convertFromString(*this, APFloat::rmNearestTiesToEven);
  if (errorToBool(StatusOrErr.takeError()))
    return true;

  APFloat::opStatus Status = *StatusOrErr;
  if (Status != APFloat::opOK) {
    if (!AllowInexact || !(Status & APFloat::opInexact))
      return true;
  }

  Result = F.convertToDouble();
  return false;
}

#endif // EXI_HAS_AP_SCALARS

#if 0
// TODO: Readd with rapidhash
// Implementation of StrRef hashing.
hash_code exi::hash_value(StrRef S) {
  return hash_combine_range(S.begin(), S.end());
}
#endif
