//===- Support/StringSaver.cpp --------------------------------------===//
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
//
// This file provides useful additions to the standard type_traits library.
//
//===----------------------------------------------------------------===//

#include <Support/StringSaver.hpp>
#include <Common/SmallStr.hpp>

using namespace exi;

StrRef StringSaver::save(StrRef S) {
  char *P = Alloc.Allocate<char>(S.size() + 1);
  if (!S.empty())
    memcpy(P, S.data(), S.size());
  P[S.size()] = '\0';
  return StrRef(P, S.size());
}

StrRef StringSaver::save(const Twine &S) {
  SmallStr<128> Storage;
  return save(S.toStrRef(Storage));
}

StrRef UniqueStringSaver::save(StrRef S) {
  auto R = Unique.insert(S);
  if (R.second) {               // cache miss, need to actually save the string
#if EXI_HAS_DENSE_SET
    *R.first = Strings.save(S); // safe replacement with equal value
#else
    Unique.emplace_hint(R.first, Strings.save(S));
#endif
  }
  return *R.first;
}

StrRef UniqueStringSaver::save(const Twine &S) {
  SmallStr<128> Storage;
  return save(S.toStrRef(Storage));
}
