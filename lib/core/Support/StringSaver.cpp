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
// Copyright (C) 2024-2025 Eightfold
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

static constexpr usize kAddNullTerm
  = kHasFlexibleArrayMembers ? 1 : 0;

ALWAYS_INLINE static InlineStr*
 CreateInlineStr(BumpPtrAllocator& Alloc, const usize Size) {
  static constexpr usize kExtra = sizeof(InlineStr) + kAddNullTerm;
  return (InlineStr*) Alloc.Allocate(Size + kExtra, alignof(InlineStr));
}

ALWAYS_INLINE static InlineStr*
 SaveWithRaw(BumpPtrAllocator& Alloc, StrRef S) {
  const usize Size = S.size();
  InlineStr *P = CreateInlineStr(Alloc, Size);
  P->Size = Size;
  if (!S.empty()) [[likely]]
    std::memcpy(P->Data, S.data(), Size);
  P->Data[Size] = '\0';
  return P;
}

ALWAYS_INLINE static StrRef
 SaveWith(BumpPtrAllocator& Alloc, StrRef S) {
  InlineStr* const P = SaveWithRaw(Alloc, S);
  return StrRef(P->Data, S.size());
}

//////////////////////////////////////////////////////////////////////////

StrRef StringSaver::save(StrRef S) {
  return SaveWith(this->Alloc, S);
}

StrRef StringSaver::save(const Twine &S) {
  SmallStr<128> Storage;
  return save(S.toStrRef(Storage));
}

InlineStr* StringSaver::saveRaw(StrRef S) {
  return SaveWithRaw(this->Alloc, S);
}

InlineStr* StringSaver::saveRaw(const Twine &S) {
  SmallStr<128> Storage;
  return saveRaw(S.toStrRef(Storage));
}

//////////////////////////////////////////////////////////////////////////

StrRef OwningStringSaver::save(StrRef S) {
  return SaveWith(this->Alloc, S);
}

StrRef OwningStringSaver::save(const Twine &S) {
  SmallStr<128> Storage;
  return save(S.toStrRef(Storage));
}

InlineStr* OwningStringSaver::saveRaw(StrRef S) {
  return SaveWithRaw(this->Alloc, S);
}

InlineStr* OwningStringSaver::saveRaw(const Twine &S) {
  SmallStr<128> Storage;
  return saveRaw(S.toStrRef(Storage));
}

//////////////////////////////////////////////////////////////////////////

StrRef UniqueStringSaver::save(StrRef S) {
  auto [It, CacheMiss] = Unique.insert(S);
  if (CacheMiss) // cache miss, need to actually save the string
    *It = Strings.save(S); // safe replacement with equal value
  return *It;
}

StrRef UniqueStringSaver::save(const Twine &S) {
  SmallStr<128> Storage;
  return save(S.toStrRef(Storage));
}
