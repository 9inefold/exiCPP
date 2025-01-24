//===- ArrayRef.cpp -------------------------------------------------===//
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
/// Implementation of the ArrayRef<char> specialization.
/// Used for strings.
///
//===----------------------------------------------------------------===//

#include <ArrayRef.hpp>
#include <Strings.hpp>

using namespace re;

template class re::ArrayRef<char>;

ALWAYS_INLINE static bool CompareArrays(
 const ArrayRef<char>& LHS, const ArrayRef<char>& RHS) noexcept {
  return Strequal(LHS.data(), RHS.data(), RHS.size());
}

ALWAYS_INLINE static bool CompareArraysInsensitive(
 const ArrayRef<char>& LHS, const ArrayRef<char>& RHS) noexcept {
  return StrequalInsensitive(LHS.data(), RHS.data(), RHS.size());
}

bool ArrayRefBase<char>::isGrEqual(ArrayRef<char> RHS) const {
  return self().size() >= RHS.size();
}

const ArrayRef<char>& ArrayRefBase<char>::self() const {
  return *static_cast<const ArrayRef<char>*>(this);
}

ArrayRef<char> ArrayRefBase<char>::New(const char* Str) {
  return ArrayRef<char>(Str, Strlen(Str));
}

ArrayRef<char> re::operator""_str(const char* Str, usize Len) noexcept {
  return ArrayRef<char>(Str, Len);
}

//////////////////////////////////////////////////////////////////////////
// Comparison

bool ArrayRefBase<char>::equals(ArrayRef<char> RHS) const {
  if (self().Length != RHS.Length)
    return false;
  return CompareArrays(self(), RHS);
}

[[nodiscard]] bool ArrayRefBase<char>::starts_with(ArrayRef<char> Prefix) const {
  return isGrEqual(Prefix)
    && CompareArrays(self(), Prefix);
}
[[nodiscard]] bool ArrayRefBase<char>::starts_with(char Prefix) const {
  return !self().empty() && self().front() == Prefix;
}

[[nodiscard]] bool ArrayRefBase<char>::starts_with_insensitive(ArrayRef<char> Prefix) const {
  return isGrEqual(Prefix)
    && CompareArraysInsensitive(self(), Prefix);
}

[[nodiscard]] bool ArrayRefBase<char>::ends_with(ArrayRef<char> Suffix) const {
  return isGrEqual(Suffix)
    && CompareArrays(
      self().take_back(Suffix.size()), Suffix);
}
[[nodiscard]] bool ArrayRefBase<char>::ends_with(char Suffix) const {
  return !self().empty() && self().back() == Suffix;
}

[[nodiscard]] bool ArrayRefBase<char>::ends_with_insensitive(ArrayRef<char> Suffix) const {
  return isGrEqual(Suffix)
    && CompareArraysInsensitive(
      self().take_back(Suffix.size()), Suffix);
}
