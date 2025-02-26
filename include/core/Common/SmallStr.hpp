//===- Common/SmallStr.hpp ------------------------------------------===//
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
/// This file defines the SmallStr class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/SmallVec.hpp>
#include <Common/String.hpp>
#include <cstddef>

namespace exi {

/// SmallStr - A SmallStr is just a SmallVec with methods and accessors
/// that make it work better as a string (e.g. operator+ etc).
template<unsigned InternalLen>
class SmallStr : public SmallVec<char_t, InternalLen> {
public:
  /// Default ctor - Initialize to empty.
  SmallStr() = default;

  /// Initialize from a StrRef.
  SmallStr(StrRef S) : SmallVec<char_t, InternalLen>(S.begin(), S.end()) {}

  /// Initialize by concatenating a list of StrRefs.
  SmallStr(std::initializer_list<StrRef> Refs)
      : SmallVec<char_t, InternalLen>() {
    this->append(Refs);
  }

  /// Initialize with a range.
  template<typename ItTy>
  SmallStr(ItTy S, ItTy E) : SmallVec<char_t, InternalLen>(S, E) {}

  /// @}
  /// @name String Assignment
  /// @{

  using SmallVec<char_t, InternalLen>::assign;

  /// Assign from a StrRef.
  void assign(StrRef RHS) {
    SmallVecImpl<char_t>::assign(RHS.begin(), RHS.end());
  }

  /// Assign from a list of StrRefs.
  void assign(std::initializer_list<StrRef> Refs) {
    this->clear();
    append(Refs);
  }

  /// @}
  /// @name String Concatenation
  /// @{

  using SmallVec<char_t, InternalLen>::append;

  /// Append from a StrRef.
  void append(StrRef RHS) {
    SmallVecImpl<char_t>::append(RHS.begin(), RHS.end());
  }

  /// Append from a list of StrRefs.
  void append(std::initializer_list<StrRef> Refs) {
    size_t CurrentSize = this->size();
    size_t SizeNeeded = CurrentSize;
    for (const StrRef &Ref : Refs)
      SizeNeeded += Ref.size();
    this->resize_for_overwrite(SizeNeeded);
    for (const StrRef &Ref : Refs) {
      std::copy(Ref.begin(), Ref.end(), this->begin() + CurrentSize);
      CurrentSize += Ref.size();
    }
    assert(CurrentSize == this->size());
  }

  /// @}
  /// @name String Comparison
  /// TODO
  /// @{

  /// Check for string equality.  This is more efficient than compare() when
  /// the relative ordering of inequal strings isn't needed.
  [[nodiscard]] bool equals(StrRef RHS) const { return str() == RHS; }

#if EXI_CUSTOM_STRREF
  /// Check for string equality, ignoring case.
  [[nodiscard]] bool equals_insensitive(StrRef RHS) const {
    return str().equals_insensitive(RHS);
  }
#endif

  /// compare - Compare two strings; the result is negative, zero, or positive
  /// if this string is lexicographically less than, equal to, or greater than
  /// the \p RHS.
  [[nodiscard]] int compare(StrRef RHS) const { return str().compare(RHS); }

#if EXI_CUSTOM_STRREF
  /// compare_insensitive - Compare two strings, ignoring case.
  [[nodiscard]] int compare_insensitive(StrRef RHS) const {
    return str().compare_insensitive(RHS);
  }

  /// compare_numeric - Compare two strings, treating sequences of digits as
  /// numbers.
  [[nodiscard]] int compare_numeric(StrRef RHS) const {
    return str().compare_numeric(RHS);
  }
#endif

  /// @}
  /// @name String Predicates
  /// @{

  /// starts_with - Check if this string starts with the given \p Prefix.
  [[nodiscard]] bool starts_with(StrRef Prefix) const {
    return str().starts_with(Prefix);
  }

  /// ends_with - Check if this string ends with the given \p Suffix.
  [[nodiscard]] bool ends_with(StrRef Suffix) const {
    return str().ends_with(Suffix);
  }

  /// @}
  /// @name String Searching
  /// @{

  /// find - Search for the first char_tacter \p C in the string.
  ///
  /// \return - The index of the first occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] size_t find(char_t C, size_t From = 0) const {
    return str().find(C, From);
  }

  /// Search for the first string \p Str in the string.
  ///
  /// \returns The index of the first occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] size_t find(StrRef Str, size_t From = 0) const {
    return str().find(Str, From);
  }

  /// Search for the last char_tacter \p C in the string.
  ///
  /// \returns The index of the last occurrence of \p C, or npos if not
  /// found.
  [[nodiscard]] size_t rfind(char_t C, size_t From = StrRef::npos) const {
    return str().rfind(C, From);
  }

  /// Search for the last string \p Str in the string.
  ///
  /// \returns The index of the last occurrence of \p Str, or npos if not
  /// found.
  [[nodiscard]] size_t rfind(StrRef Str) const { return str().rfind(Str); }

  /// Find the first char_tacter in the string that is \p C, or npos if not
  /// found. Same as find.
  [[nodiscard]] size_t find_first_of(char_t C, size_t From = 0) const {
    return str().find_first_of(C, From);
  }

  /// Find the first char_tacter in the string that is in \p Chars, or npos if
  /// not found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] size_t find_first_of(StrRef Chars, size_t From = 0) const {
    return str().find_first_of(Chars, From);
  }

  /// Find the first char_tacter in the string that is not \p C or npos if not
  /// found.
  [[nodiscard]] size_t find_first_not_of(char_t C, size_t From = 0) const {
    return str().find_first_not_of(C, From);
  }

  /// Find the first char_tacter in the string that is not in the string
  /// \p Chars, or npos if not found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] size_t find_first_not_of(StrRef Chars,
                                         size_t From = 0) const {
    return str().find_first_not_of(Chars, From);
  }

  /// Find the last char_tacter in the string that is \p C, or npos if not
  /// found.
  [[nodiscard]] size_t find_last_of(char_t C,
                                    size_t From = StrRef::npos) const {
    return str().find_last_of(C, From);
  }

  /// Find the last char_tacter in the string that is in \p C, or npos if not
  /// found.
  ///
  /// Complexity: O(size() + Chars.size())
  [[nodiscard]] size_t find_last_of(StrRef Chars,
                                    size_t From = StrRef::npos) const {
    return str().find_last_of(Chars, From);
  }

  /// @}
  /// @name Helpful Algorithms
  /// @{

  /// Return the number of occurrences of \p C in the string.
  [[nodiscard]] size_t count(char_t C) const { return str().count(C); }

  /// Return the number of non-overlapped occurrences of \p Str in the
  /// string.
  [[nodiscard]] size_t count(StrRef Str) const { return str().count(Str); }

  /// @}
  /// @name Substring Operations
  /// @{

  /// Return a reference to the substring from [Start, Start + N).
  ///
  /// \param Start The index of the starting char_tacter in the substring; if
  /// the index is npos or greater than the length of the string then the
  /// empty substring will be returned.
  ///
  /// \param N The number of char_tacters to included in the substring. If \p N
  /// exceeds the number of char_tacters remaining in the string, the string
  /// suffix (starting with \p Start) will be returned.
  [[nodiscard]] StrRef substr(size_t Start,
                                 size_t N = StrRef::npos) const {
    return str().substr(Start, N);
  }

  /// Return a reference to the substring from [Start, End).
  ///
  /// \param Start The index of the starting char_tacter in the substring; if
  /// the index is npos or greater than the length of the string then the
  /// empty substring will be returned.
  ///
  /// \param End The index following the last char_tacter to include in the
  /// substring. If this is npos, or less than \p Start, or exceeds the
  /// number of char_tacters remaining in the string, the string suffix
  /// (starting with \p Start) will be returned.
  [[nodiscard]] StrRef slice(size_t Start, size_t End) const {
    return str().slice(Start, End);
  }

  // Extra methods.

  /// Explicit conversion to StrRef.
  [[nodiscard]] StrRef str() const {
    return StrRef(this->data(), this->size());
  }

  // TODO: Make this const, if it's safe...
  const char_t* c_str() {
    this->push_back(0);
    this->pop_back();
    return this->data();
  }

  /// Null terminates and returns a str().
  [[nodiscard]] StrRef n_str() {
    return StrRef(this->c_str(), this->size());
  }

  /// Implicit conversion to StrRef.
  operator StrRef() const { return str(); }

  explicit operator String() const {
    return String(this->data(), this->size());
  }

  // Extra operators.
  SmallStr &operator=(StrRef RHS) {
    this->assign(RHS);
    return *this;
  }

  SmallStr &operator+=(StrRef RHS) {
    this->append(RHS.begin(), RHS.end());
    return *this;
  }
  SmallStr &operator+=(char_t C) {
    this->push_back(C);
    return *this;
  }
};

} // end namespace exi
