//===- Common/StrRef.hpp --------------------------------------------===//
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

#pragma once

// #include "llvm/ADT/DenseMapInfo.h"
#include <Common/_Str.hpp>
#include <Common/FunctionRef.hpp>
#include <Common/Fundamental.hpp>
#include <Common/iterator_range.hpp>
#include <Config/FeatureFlags.hpp>
#include <Support/ErrorHandle.hpp>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <fmt/core.h>

namespace exi {

#if EXI_HAS_AP_SCALARS
class APInt;
#endif

class hash_code;
template <typename T> class SmallVecImpl;
class StrRef;

/// Helper functions for StrRef::getAsInteger.
bool getAsUnsignedInteger(StrRef Str, unsigned Radix,
                          unsigned long long &Result);

bool getAsSignedInteger(StrRef Str, unsigned Radix, long long &Result);

bool consumeUnsignedInteger(StrRef &Str, unsigned Radix,
                            unsigned long long &Result);
bool consumeSignedInteger(StrRef &Str, unsigned Radix, long long &Result);

/// StrRef - Represent a constant reference to a string, i.e. a character
/// array and a length, which need not be null terminated.
///
/// This class does not own the string data, it is expected to be used in
/// situations where the character data resides in some other buffer, whose
/// lifetime extends past that of the StrRef. For this reason, it is not in
/// general safe to store a StrRef.
class EXI_GSL_POINTER StrRef {
public:
  static constexpr usize npos = ~usize(0);

  using iterator = const char *;
  using const_iterator = const char *;
  using size_type = usize;
  using value_type = char;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
  /// The start of the string, in an external buffer.
  const char *Data = nullptr;

  /// The length of the string.
  usize Length = 0;

  // Workaround memcmp issue with null pointers (undefined behavior)
  // by providing a specialized version
  static int compareMemory(const char *Lhs, const char *Rhs, usize Length) {
    if (Length == 0) { return 0; }
    return ::memcmp(Lhs,Rhs,Length);
  }

public:
  /// @name Constructors
  /// @{

  /// Construct an empty string ref.
  /*implicit*/ StrRef() = default;

  /// Disable conversion from nullptr.  This prevents things like
  /// if (Str == nullptr)
  StrRef(std::nullptr_t) = delete;

  /// Construct a string ref from a cstring.
  /*implicit*/ constexpr StrRef(const char *Str EXI_LIFETIMEBOUND)
      : Data(Str), Length(Str ?
  // GCC 7 doesn't have constexpr char_traits. Fall back to __builtin_strlen.
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE < 8
                              __builtin_strlen(Str)
#else
                              std::char_traits<char>::length(Str)
#endif
                              : 0) {
  }

  /// Construct a string ref from a pointer and length.
  /*implicit*/ constexpr StrRef(const char *data EXI_LIFETIMEBOUND,
                                   usize length)
      : Data(data), Length(length) {}

  /// Construct a string ref from a String.
  /*implicit*/ StrRef(const String &Str)
      : Data(Str.data()), Length(Str.length()) {}

  /// Construct a string ref from an std::string_view.
  /*implicit*/ constexpr StrRef(std::string_view Str)
      : Data(Str.data()), Length(Str.size()) {}

  /// @}
  /// @name Iterators
  /// @{

  iterator begin() const { return data(); }

  iterator end() const { return data() + size(); }

  reverse_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }

  reverse_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  const unsigned char *bytes_begin() const {
    return reinterpret_cast<const unsigned char *>(begin());
  }
  const unsigned char *bytes_end() const {
    return reinterpret_cast<const unsigned char *>(end());
  }
  iterator_range<const unsigned char *> bytes() const {
    return make_range(bytes_begin(), bytes_end());
  }

  /// @}
  /// @name String Operations
  /// @{

  /// data - Get a pointer to the start of the string (which may not be null
  /// terminated).
  [[nodiscard]] constexpr const char *data() const { return Data; }

  /// empty - Check if the string is empty.
  [[nodiscard]] constexpr bool empty() const { return size() == 0; }

  /// size - Get the string size.
  [[nodiscard]] constexpr usize size() const { return Length; }

  /// front - Get the first character in the string.
  [[nodiscard]] char front() const {
    exi_assert(!empty());
    return data()[0];
  }

  /// back - Get the last character in the string.
  [[nodiscard]] char back() const {
    exi_assert(!empty());
    return data()[size() - 1];
  }

  // copy - Allocate copy in Allocator and return StrRef to it.
  template <typename Allocator>
  [[nodiscard]] StrRef copy(Allocator &A) const {
    // Don't request a length 0 copy from the allocator.
    if (empty())
      return StrRef();
    char *Str = A.template Allocate<char>(size());
    std::copy(begin(), end(), Str);
    return StrRef(Str, size());
  }

  /// Check for string equality, ignoring case.
  [[nodiscard]] bool equals_insensitive(StrRef RHS) const {
    return size() == RHS.size() && compare_insensitive(RHS) == 0;
  }

  /// compare - Compare two strings; the result is negative, zero, or positive
  /// if this string is lexicographically less than, equal to, or greater than
  /// the \p RHS.
  [[nodiscard]] int compare(StrRef RHS) const {
    // Check the prefix for a mismatch.
    if (int Res =
            compareMemory(data(), RHS.data(), std::min(size(), RHS.size())))
      return Res < 0 ? -1 : 1;

    // Otherwise the prefixes match, so we only need to check the lengths.
    if (size() == RHS.size())
      return 0;
    return size() < RHS.size() ? -1 : 1;
  }

  /// Compare two strings, ignoring case.
  [[nodiscard]] int compare_insensitive(StrRef RHS) const;

  /// compare_numeric - Compare two strings, treating sequences of digits as
  /// numbers.
  [[nodiscard]] int compare_numeric(StrRef RHS) const;

  /// Determine the edit distance between this string and another
  /// string.
  ///
  /// \param Other the string to compare this string against.
  ///
  /// \param AllowReplacements whether to allow character
  /// replacements (change one character into another) as a single
  /// operation, rather than as two operations (an insertion and a
  /// removal).
  ///
  /// \param MaxEditDistance If non-zero, the maximum edit distance that
  /// this routine is allowed to compute. If the edit distance will exceed
  /// that maximum, returns \c MaxEditDistance+1.
  ///
  /// \returns the minimum number of character insertions, removals,
  /// or (if \p AllowReplacements is \c true) replacements needed to
  /// transform one of the given strings into the other. If zero,
  /// the strings are identical.
  [[nodiscard]] unsigned edit_distance(StrRef Other,
                                       bool AllowReplacements = true,
                                       unsigned MaxEditDistance = 0) const;

  [[nodiscard]] unsigned
  edit_distance_insensitive(StrRef Other, bool AllowReplacements = true,
                            unsigned MaxEditDistance = 0) const;

  /// str - Get the contents as an String.
  [[nodiscard]] String str() const {
    if (!data())
      return String();
    return String(data(), size());
  }

  /// @}
  /// @name Operator Overloads
  /// @{

  [[nodiscard]] char operator[](usize Index) const {
    exi_assert(Index < size(), "Invalid index!");
    return data()[Index];
  }

  /// Disallow accidental assignment from a temporary String.
  ///
  /// The declaration here is extra complicated so that `stringRef = {}`
  /// and `stringRef = "abc"` continue to select the move assignment operator.
  template <typename T>
  std::enable_if_t<std::is_same<T, String>::value, StrRef> &
  operator=(T &&Str) = delete;

  /// @}
  /// @name Type Conversions
  /// @{

  constexpr operator std::string_view() const {
    return std::string_view(data(), size());
  }

  /// @}
  /// @name String Predicates
  /// @{

  /// Check if this string starts with the given \p Prefix.
  [[nodiscard]] bool starts_with(StrRef Prefix) const {
    return size() >= Prefix.size() &&
           compareMemory(data(), Prefix.data(), Prefix.size()) == 0;
  }
  [[nodiscard]] bool starts_with(char Prefix) const {
    return !empty() && front() == Prefix;
  }

  /// Check if this string starts with the given \p Prefix, ignoring case.
  [[nodiscard]] bool starts_with_insensitive(StrRef Prefix) const;

  /// Check if this string ends with the given \p Suffix.
  [[nodiscard]] bool ends_with(StrRef Suffix) const {
    return size() >= Suffix.size() &&
           compareMemory(end() - Suffix.size(), Suffix.data(),
                         Suffix.size()) == 0;
  }
  [[nodiscard]] bool ends_with(char Suffix) const {
    return !empty() && back() == Suffix;
  }

  /// Check if this string ends with the given \p Suffix, ignoring case.
  [[nodiscard]] bool ends_with_insensitive(StrRef Suffix) const;

  /// @}
  /// @name String Searching
  /// @{

  /// Search for the first character \p C in the string.
  ///
  /// \returns The index of the first occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] usize find(char C, usize From = 0) const {
    return std::string_view(*this).find(C, From);
  }

  /// Search for the first character \p C in the string, ignoring case.
  ///
  /// \returns The index of the first occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] usize find_insensitive(char C, usize From = 0) const;

  /// Search for the first character satisfying the predicate \p F
  ///
  /// \returns The index of the first character satisfying \p F starting from
  /// \p From, or npos if not found.
  [[nodiscard]] usize find_if(function_ref<bool(char)> F,
                               usize From = 0) const {
    StrRef Str = drop_front(From);
    while (!Str.empty()) {
      if (F(Str.front()))
        return size() - Str.size();
      Str = Str.drop_front();
    }
    return npos;
  }

  /// Search for the first character not satisfying the predicate \p F
  ///
  /// \returns The index of the first character not satisfying \p F starting
  /// from \p From, or npos if not found.
  [[nodiscard]] usize find_if_not(function_ref<bool(char)> F,
                                   usize From = 0) const {
    return find_if([F](char c) { return !F(c); }, From);
  }

  /// Search for the first string \p Str in the string.
  ///
  /// \returns The index of the first occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] usize find(StrRef Str, usize From = 0) const;

  /// Search for the first string \p Str in the string, ignoring case.
  ///
  /// \returns The index of the first occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] usize find_insensitive(StrRef Str, usize From = 0) const;

  /// Search for the last character \p C in the string.
  ///
  /// \returns The index of the last occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] usize rfind(char C, usize From = npos) const {
    usize I = std::min(From, size());
    while (I) {
      --I;
      if (data()[I] == C)
        return I;
    }
    return npos;
  }

  /// Search for the last character \p C in the string, ignoring case.
  ///
  /// \returns The index of the last occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] usize rfind_insensitive(char C, usize From = npos) const;

  /// Search for the last string \p Str in the string.
  ///
  /// \returns The index of the last occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] usize rfind(StrRef Str) const;

  /// Search for the last string \p Str in the string, ignoring case.
  ///
  /// \returns The index of the last occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] usize rfind_insensitive(StrRef Str) const;

  /// Find the first character in the string that is \p C, or npos if not
  /// found. Same as find.
  [[nodiscard]] usize find_first_of(char C, usize From = 0) const {
    return find(C, From);
  }

  /// Find the first character in the string that is in \p Chars, or npos if
  /// not found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] usize find_first_of(StrRef Chars, usize From = 0) const;

  /// Find the first character in the string that is not \p C or npos if not
  /// found.
  [[nodiscard]] usize find_first_not_of(char C, usize From = 0) const;

  /// Find the first character in the string that is not in the string
  /// \p Chars, or npos if not found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] usize find_first_not_of(StrRef Chars,
                                         usize From = 0) const;

  /// Find the last character in the string that is \p C, or npos if not
  /// found.
  [[nodiscard]] usize find_last_of(char C, usize From = npos) const {
    return rfind(C, From);
  }

  /// Find the last character in the string that is in \p C, or npos if not
  /// found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] usize find_last_of(StrRef Chars,
                                    usize From = npos) const;

  /// Find the last character in the string that is not \p C, or npos if not
  /// found.
  [[nodiscard]] usize find_last_not_of(char C, usize From = npos) const;

  /// Find the last character in the string that is not in \p Chars, or
  /// npos if not found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] usize find_last_not_of(StrRef Chars,
                                        usize From = npos) const;

  /// Return true if the given string is a substring of *this, and false
  /// otherwise.
  [[nodiscard]] bool contains(StrRef Other) const {
    return find(Other) != npos;
  }

  /// Return true if the given character is contained in *this, and false
  /// otherwise.
  [[nodiscard]] bool contains(char C) const {
    return find_first_of(C) != npos;
  }

  /// Return true if the given string is a substring of *this, and false
  /// otherwise.
  [[nodiscard]] bool contains_insensitive(StrRef Other) const {
    return find_insensitive(Other) != npos;
  }

  /// Return true if the given character is contained in *this, and false
  /// otherwise.
  [[nodiscard]] bool contains_insensitive(char C) const {
    return find_insensitive(C) != npos;
  }

  /// @}
  /// @name Helpful Algorithms
  /// @{

  /// Return the number of occurrences of \p C in the string.
  [[nodiscard]] usize count(char C) const {
    usize Count = 0;
    for (usize I = 0; I != size(); ++I)
      if (data()[I] == C)
        ++Count;
    return Count;
  }

  /// Return the number of non-overlapped occurrences of \p Str in
  /// the string.
  usize count(StrRef Str) const;

  /// Parse the current string as an integer of the specified radix.  If
  /// \p Radix is specified as zero, this does radix autosensing using
  /// extended C rules: 0 is octal, 0x is hex, 0b is binary.
  ///
  /// If the string is invalid or if only a subset of the string is valid,
  /// this returns true to signify the error.  The string is considered
  /// erroneous if empty or if it overflows T.
  template <typename T> bool getAsInteger(unsigned Radix, T &Result) const {
    if constexpr (std::numeric_limits<T>::is_signed) {
      long long LLVal;
      if (getAsSignedInteger(*this, Radix, LLVal) ||
          static_cast<T>(LLVal) != LLVal)
        return true;
      Result = LLVal;
    } else {
      unsigned long long ULLVal;
      // The additional cast to unsigned long long is required to avoid the
      // Visual C++ warning C4805: '!=' : unsafe mix of type 'bool' and type
      // 'unsigned __int64' when instantiating getAsInteger with T = bool.
      if (getAsUnsignedInteger(*this, Radix, ULLVal) ||
          static_cast<unsigned long long>(static_cast<T>(ULLVal)) != ULLVal)
        return true;
      Result = ULLVal;
    }
    return false;
  }

  /// Parse the current string as an integer of the specified radix.  If
  /// \p Radix is specified as zero, this does radix autosensing using
  /// extended C rules: 0 is octal, 0x is hex, 0b is binary.
  ///
  /// If the string does not begin with a number of the specified radix,
  /// this returns true to signify the error. The string is considered
  /// erroneous if empty or if it overflows T.
  /// The portion of the string representing the discovered numeric value
  /// is removed from the beginning of the string.
  template <typename T> bool consumeInteger(unsigned Radix, T &Result) {
    if constexpr (std::numeric_limits<T>::is_signed) {
      long long LLVal;
      if (consumeSignedInteger(*this, Radix, LLVal) ||
          static_cast<long long>(static_cast<T>(LLVal)) != LLVal)
        return true;
      Result = LLVal;
    } else {
      unsigned long long ULLVal;
      if (consumeUnsignedInteger(*this, Radix, ULLVal) ||
          static_cast<unsigned long long>(static_cast<T>(ULLVal)) != ULLVal)
        return true;
      Result = ULLVal;
    }
    return false;
  }

#if EXI_HAS_AP_SCALARS
  /// Parse the current string as an integer of the specified \p Radix, or of
  /// an autosensed radix if the \p Radix given is 0.  The current value in
  /// \p Result is discarded, and the storage is changed to be wide enough to
  /// store the parsed integer.
  ///
  /// \returns true if the string does not solely consist of a valid
  /// non-empty number in the appropriate base.
  ///
  /// APInt::fromString is superficially similar but assumes the
  /// string is well-formed in the given radix.
  bool getAsInteger(unsigned Radix, APInt &Result) const;

  /// Parse the current string as an integer of the specified \p Radix.  If
  /// \p Radix is specified as zero, this does radix autosensing using
  /// extended C rules: 0 is octal, 0x is hex, 0b is binary.
  ///
  /// If the string does not begin with a number of the specified radix,
  /// this returns true to signify the error. The string is considered
  /// erroneous if empty.
  /// The portion of the string representing the discovered numeric value
  /// is removed from the beginning of the string.
  bool consumeInteger(unsigned Radix, APInt &Result);

  /// Parse the current string as an IEEE double-precision floating
  /// point value.  The string must be a well-formed double.
  ///
  /// If \p AllowInexact is false, the function will fail if the string
  /// cannot be represented exactly.  Otherwise, the function only fails
  /// in case of an overflow or underflow, or an invalid floating point
  /// representation.
  bool getAsDouble(double &Result, bool AllowInexact = true) const;
#endif // EXI_HAS_AP_SCALARS

  /// @}
  /// @name String Operations
  /// @{

  // Convert the given ASCII string to lowercase.
  [[nodiscard]] String lower() const;

  /// Convert the given ASCII string to uppercase.
  [[nodiscard]] String upper() const;

  /// @}
  /// @name Substring Operations
  /// @{

  /// Return a reference to the substring from [Start, Start + N).
  ///
  /// \param Start The index of the starting character in the substring; if
  /// the index is npos or greater than the length of the string then the
  /// empty substring will be returned.
  ///
  /// \param N The number of characters to included in the substring. If N
  /// exceeds the number of characters remaining in the string, the string
  /// suffix (starting with \p Start) will be returned.
  [[nodiscard]] constexpr StrRef substr(usize Start,
                                           usize N = npos) const {
    Start = std::min(Start, size());
    return StrRef(data() + Start, std::min(N, size() - Start));
  }

  /// Return a StrRef equal to 'this' but with only the first \p N
  /// elements remaining.  If \p N is greater than the length of the
  /// string, the entire string is returned.
  [[nodiscard]] StrRef take_front(usize N = 1) const {
    if (N >= size())
      return *this;
    return drop_back(size() - N);
  }

  /// Return a StrRef equal to 'this' but with only the last \p N
  /// elements remaining.  If \p N is greater than the length of the
  /// string, the entire string is returned.
  [[nodiscard]] StrRef take_back(usize N = 1) const {
    if (N >= size())
      return *this;
    return drop_front(size() - N);
  }

  /// Return the longest prefix of 'this' such that every character
  /// in the prefix satisfies the given predicate.
  [[nodiscard]] StrRef take_while(function_ref<bool(char)> F) const {
    return substr(0, find_if_not(F));
  }

  /// Return the longest prefix of 'this' such that no character in
  /// the prefix satisfies the given predicate.
  [[nodiscard]] StrRef take_until(function_ref<bool(char)> F) const {
    return substr(0, find_if(F));
  }

  /// Return a StrRef equal to 'this' but with the first \p N elements
  /// dropped.
  [[nodiscard]] StrRef drop_front(usize N = 1) const {
    exi_assert(size() >= N, "Dropping more elements than exist");
    return substr(N);
  }

  /// Return a StrRef equal to 'this' but with the last \p N elements
  /// dropped.
  [[nodiscard]] StrRef drop_back(usize N = 1) const {
    exi_assert(size() >= N, "Dropping more elements than exist");
    return substr(0, size()-N);
  }

  /// Return a StrRef equal to 'this', but with all characters satisfying
  /// the given predicate dropped from the beginning of the string.
  [[nodiscard]] StrRef drop_while(function_ref<bool(char)> F) const {
    return substr(find_if_not(F));
  }

  /// Return a StrRef equal to 'this', but with all characters not
  /// satisfying the given predicate dropped from the beginning of the string.
  [[nodiscard]] StrRef drop_until(function_ref<bool(char)> F) const {
    return substr(find_if(F));
  }

  /// Returns true if this StrRef has the given prefix and removes that
  /// prefix.
  bool consume_front(StrRef Prefix) {
    if (!starts_with(Prefix))
      return false;

    *this = substr(Prefix.size());
    return true;
  }

  /// Returns true if this StrRef has the given prefix, ignoring case,
  /// and removes that prefix.
  bool consume_front_insensitive(StrRef Prefix) {
    if (!starts_with_insensitive(Prefix))
      return false;

    *this = substr(Prefix.size());
    return true;
  }

  /// Returns true if this StrRef has the given suffix and removes that
  /// suffix.
  bool consume_back(StrRef Suffix) {
    if (!ends_with(Suffix))
      return false;

    *this = substr(0, size() - Suffix.size());
    return true;
  }

  /// Returns true if this StrRef has the given suffix, ignoring case,
  /// and removes that suffix.
  bool consume_back_insensitive(StrRef Suffix) {
    if (!ends_with_insensitive(Suffix))
      return false;

    *this = substr(0, size() - Suffix.size());
    return true;
  }

  /// Return a reference to the substring from [Start, End).
  ///
  /// \param Start The index of the starting character in the substring; if
  /// the index is npos or greater than the length of the string then the
  /// empty substring will be returned.
  ///
  /// \param End The index following the last character to include in the
  /// substring. If this is npos or exceeds the number of characters
  /// remaining in the string, the string suffix (starting with \p Start)
  /// will be returned. If this is less than \p Start, an empty string will
  /// be returned.
  [[nodiscard]] StrRef slice(usize Start, usize End) const {
    Start = std::min(Start, size());
    End = std::clamp(End, Start, size());
    return StrRef(data() + Start, End - Start);
  }

  /// Split into two substrings around the first occurrence of a separator
  /// character.
  ///
  /// If \p Separator is in the string, then the result is a pair (LHS, RHS)
  /// such that (*this == LHS + Separator + RHS) is true and RHS is
  /// maximal. If \p Separator is not in the string, then the result is a
  /// pair (LHS, RHS) where (*this == LHS) and (RHS == "").
  ///
  /// \param Separator The character to split on.
  /// \returns The split substrings.
  [[nodiscard]] std::pair<StrRef, StrRef> split(char Separator) const {
    return split(StrRef(&Separator, 1));
  }

  /// Split into two substrings around the first occurrence of a separator
  /// string.
  ///
  /// If \p Separator is in the string, then the result is a pair (LHS, RHS)
  /// such that (*this == LHS + Separator + RHS) is true and RHS is
  /// maximal. If \p Separator is not in the string, then the result is a
  /// pair (LHS, RHS) where (*this == LHS) and (RHS == "").
  ///
  /// \param Separator - The string to split on.
  /// \return - The split substrings.
  [[nodiscard]] std::pair<StrRef, StrRef>
  split(StrRef Separator) const {
    usize Idx = find(Separator);
    if (Idx == npos)
      return std::make_pair(*this, StrRef());
    return std::make_pair(slice(0, Idx), substr(Idx + Separator.size()));
  }

  /// Split into two substrings around the last occurrence of a separator
  /// string.
  ///
  /// If \p Separator is in the string, then the result is a pair (LHS, RHS)
  /// such that (*this == LHS + Separator + RHS) is true and RHS is
  /// minimal. If \p Separator is not in the string, then the result is a
  /// pair (LHS, RHS) where (*this == LHS) and (RHS == "").
  ///
  /// \param Separator - The string to split on.
  /// \return - The split substrings.
  [[nodiscard]] std::pair<StrRef, StrRef>
  rsplit(StrRef Separator) const {
    usize Idx = rfind(Separator);
    if (Idx == npos)
      return std::make_pair(*this, StrRef());
    return std::make_pair(slice(0, Idx), substr(Idx + Separator.size()));
  }

  /// Split into substrings around the occurrences of a separator string.
  ///
  /// Each substring is stored in \p A. If \p MaxSplit is >= 0, at most
  /// \p MaxSplit splits are done and consequently <= \p MaxSplit + 1
  /// elements are added to A.
  /// If \p KeepEmpty is false, empty strings are not added to \p A. They
  /// still count when considering \p MaxSplit
  /// An useful invariant is that
  /// Separator.join(A) == *this if MaxSplit == -1 and KeepEmpty == true
  ///
  /// \param A - Where to put the substrings.
  /// \param Separator - The string to split on.
  /// \param MaxSplit - The maximum number of times the string is split.
  /// \param KeepEmpty - True if empty substring should be added.
  void split(SmallVecImpl<StrRef> &A,
             StrRef Separator, int MaxSplit = -1,
             bool KeepEmpty = true) const;

  /// Split into substrings around the occurrences of a separator character.
  ///
  /// Each substring is stored in \p A. If \p MaxSplit is >= 0, at most
  /// \p MaxSplit splits are done and consequently <= \p MaxSplit + 1
  /// elements are added to A.
  /// If \p KeepEmpty is false, empty strings are not added to \p A. They
  /// still count when considering \p MaxSplit
  /// An useful invariant is that
  /// Separator.join(A) == *this if MaxSplit == -1 and KeepEmpty == true
  ///
  /// \param A - Where to put the substrings.
  /// \param Separator - The string to split on.
  /// \param MaxSplit - The maximum number of times the string is split.
  /// \param KeepEmpty - True if empty substring should be added.
  void split(SmallVecImpl<StrRef> &A, char Separator, int MaxSplit = -1,
             bool KeepEmpty = true) const;

  /// Split into two substrings around the last occurrence of a separator
  /// character.
  ///
  /// If \p Separator is in the string, then the result is a pair (LHS, RHS)
  /// such that (*this == LHS + Separator + RHS) is true and RHS is
  /// minimal. If \p Separator is not in the string, then the result is a
  /// pair (LHS, RHS) where (*this == LHS) and (RHS == "").
  ///
  /// \param Separator - The character to split on.
  /// \return - The split substrings.
  [[nodiscard]] std::pair<StrRef, StrRef> rsplit(char Separator) const {
    return rsplit(StrRef(&Separator, 1));
  }

  /// Return string with consecutive \p Char characters starting from the
  /// the left removed.
  [[nodiscard]] StrRef ltrim(char Char) const {
    return drop_front(std::min(size(), find_first_not_of(Char)));
  }

  /// Return string with consecutive characters in \p Chars starting from
  /// the left removed.
  [[nodiscard]] StrRef ltrim(StrRef Chars = " \t\n\v\f\r") const {
    return drop_front(std::min(size(), find_first_not_of(Chars)));
  }

  /// Return string with consecutive \p Char characters starting from the
  /// right removed.
  [[nodiscard]] StrRef rtrim(char Char) const {
    return drop_back(size() - std::min(size(), find_last_not_of(Char) + 1));
  }

  /// Return string with consecutive characters in \p Chars starting from
  /// the right removed.
  [[nodiscard]] StrRef rtrim(StrRef Chars = " \t\n\v\f\r") const {
    return drop_back(size() - std::min(size(), find_last_not_of(Chars) + 1));
  }

  /// Return string with consecutive \p Char characters starting from the
  /// left and right removed.
  [[nodiscard]] StrRef trim(char Char) const {
    return ltrim(Char).rtrim(Char);
  }

  /// Return string with consecutive characters in \p Chars starting from
  /// the left and right removed.
  [[nodiscard]] StrRef trim(StrRef Chars = " \t\n\v\f\r") const {
    return ltrim(Chars).rtrim(Chars);
  }

  /// Detect the line ending style of the string.
  ///
  /// If the string contains a line ending, return the line ending character
  /// sequence that is detected. Otherwise return '\n' for unix line endings.
  ///
  /// \return - The line ending character sequence.
  [[nodiscard]] StrRef detectEOL() const {
    usize Pos = find('\r');
    if (Pos == npos) {
      // If there is no carriage return, assume unix
      return "\n";
    }
    if (Pos + 1 < size() && data()[Pos + 1] == '\n')
      return "\r\n"; // Windows
    if (Pos > 0 && data()[Pos - 1] == '\n')
      return "\n\r"; // You monster!
    return "\r";     // Classic Mac
  }
  /// @}
};

/// @name StrRef Comparison Operators
/// @{

inline bool operator==(StrRef LHS, StrRef RHS) {
  if (LHS.size() != RHS.size())
    return false;
  if (LHS.empty())
    return true;
  return ::memcmp(LHS.data(), RHS.data(), LHS.size()) == 0;
}

inline bool operator!=(StrRef LHS, StrRef RHS) { return !(LHS == RHS); }

inline bool operator<(StrRef LHS, StrRef RHS) {
  return LHS.compare(RHS) < 0;
}

inline bool operator<=(StrRef LHS, StrRef RHS) {
  return LHS.compare(RHS) <= 0;
}

inline bool operator>(StrRef LHS, StrRef RHS) {
  return LHS.compare(RHS) > 0;
}

inline bool operator>=(StrRef LHS, StrRef RHS) {
  return LHS.compare(RHS) >= 0;
}

inline String &operator+=(String &buffer, StrRef string) {
  return buffer.append(string.data(), string.size());
}

/// @}

/// Compute a hash_code for a StrRef.
[[nodiscard]] hash_code hash_value(StrRef Str);

/// A wrapper around a string literal that serves as a proxy for constructing
/// global tables of StringRefs with the length computed at compile time.
/// In order to avoid the invocation of a global constructor, StringLiteral
/// should *only* be used in a constexpr context, as such:
///
/// constexpr StringLiteral Str("test");
///
class StringLiteral : public StrRef {
private:
  constexpr StringLiteral(const char* Str, usize N) : StrRef(Str, N) {}
public:
  template <usize N>
  consteval StringLiteral(const char(&Str)[N])
#if defined(__clang__) && __has_attribute(enable_if)
DIAGNOSTIC_PUSH()
CLANG_IGNORED("-Wgcc-compat")
      __attribute((enable_if(__builtin_strlen(Str) == N - 1,
                             "invalid string literal")))
DIAGNOSTIC_POP()
#endif
      : StrRef(Str, N - 1) {
  }

  // Explicit construction for strings like "foo\0bar".
  template <usize N>
  static consteval StringLiteral WithInnerNUL(const char(&Str)[N]) {
    return StringLiteral(Str, N - 1);
  }
};

} // namespace exi

namespace fmt {

FMT_FORMAT_AS(exi::StrRef, std::string_view);

} // namespace fmt

namespace std {

template <> struct hash<exi::StrRef> : hash<std::string_view> {
  using hash<std::string_view>::operator();
};

template <> struct hash<exi::StringLiteral> : hash<exi::StrRef> {
  using hash<exi::StrRef>::operator();
};

} // namespace std
