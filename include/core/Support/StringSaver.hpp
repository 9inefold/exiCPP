//===- Support/StringSaver.hpp --------------------------------------===//
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

#include <Config/FeatureFlags.hpp>
#if EXI_HAS_DENSE_SET
# include <Common/DenseSet.hpp>
#else
# include <Common/Set.hpp>
#endif
#include <Common/StrRef.hpp>
#include <Common/Twine.hpp>
#include <Support/Allocator.hpp>

namespace exi {

/// Saves strings in the provided stable storage and returns a
/// StrRef with a stable character pointer.
class StringSaver final {
  BumpPtrAllocator &Alloc;

public:
  StringSaver(BumpPtrAllocator &Alloc) : Alloc(Alloc) {}

  BumpPtrAllocator &getAllocator() const { return Alloc; }

  // All returned strings are null-terminated: *save(S).end() == 0.
  StrRef save(const char *S) { return save(StrRef(S)); }
  StrRef save(StrRef S);
  StrRef save(const Twine &S);
  StrRef save(const std::string &S) { return save(StrRef(S)); }
};

/// Saves strings in the provided stable storage and returns a StrRef with a
/// stable character pointer. Saving the same string yields the same StrRef.
///
/// Compared to StringSaver, it does more work but avoids saving the same string
/// multiple times.
///
/// Compared to StringPool, it performs fewer allocations but doesn't support
/// refcounting/deletion.
class UniqueStringSaver final {
  StringSaver Strings;
#if EXI_HAS_DENSE_SET
  exi::DenseSet<exi::StrRef> Unique;
#else
  exi::Set<exi::StrRef> Unique;
#endif

public:
  UniqueStringSaver(BumpPtrAllocator &Alloc) : Strings(Alloc) {}

  // All returned strings are null-terminated: *save(S).end() == 0.
  StrRef save(const char *S) { return save(StrRef(S)); }
  StrRef save(StrRef S);
  StrRef save(const Twine &S);
  StrRef save(const std::string &S) { return save(StrRef(S)); }
};

} // namespace exi
