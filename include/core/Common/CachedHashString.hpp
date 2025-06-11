//===- Common/CachedHashString.hpp ----------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines CachedHashString and CachedHashStrRef.  These are
/// owning and not-owning string types that store their hash in addition to
/// their string data.
///
/// Unlike std::string, CachedHashString can be used in DenseSet/DenseMap
/// (because, unlike std::string, CachedHashString lets us have empty and
/// tombstone values).
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/DenseMapInfo.hpp>
#include <Common/Fundamental.hpp>
#include <Common/SimpleArray.hpp>
#include <Common/StrRef.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Limits.hpp>

/// May use this for some validation later?
#define EXI_CACHEDHASHSTRING_UNIQUE_ZERO 0

// TODO: Add CachedHashString tests

namespace exi {
namespace H {

template <typename CHS>
ALWAYS_INLINE unsigned GetCHSHashValue(const CHS& S) {
  using IT = DenseMapInfo<CHS>;
  exi_assert(!IT::isEqual(S, IT::getEmptyKey()), "Cannot hash the empty key!");
  exi_assert(!IT::isEqual(S, IT::getTombstoneKey()), "Cannot hash the tombstone key!");
  return S.hash();
}

} // namespace H

/// A container which contains a `StrRef` plus a precomputed hash.
class CachedHashStrRef {
  const char* P;
  u32 Size;
  u32 Hash;

public:
  // Explicit because hashing a string isn't free.
  explicit CachedHashStrRef(StrRef S)
      : CachedHashStrRef(S, DenseMapInfo<StrRef>::getHashValue(S)) {}

  CachedHashStrRef(StrRef S, u32 Hash)
      : P(S.data()), Size(S.size()), Hash(Hash) {
    exi_assert(S.size() <= exi::max_v<u32>);
  }

  StrRef val() const { return StrRef(P, Size); }
  const char* data() const { return P; }
  u32 size() const { return Size; }
  u32 hash() const { return Hash; }
};

template <> struct DenseMapInfo<CachedHashStrRef> {
  static CachedHashStrRef getEmptyKey() {
    return CachedHashStrRef(DenseMapInfo<StrRef>::getEmptyKey(), 0);
  }
  static CachedHashStrRef getTombstoneKey() {
    return CachedHashStrRef(DenseMapInfo<StrRef>::getTombstoneKey(), 1);
  }
  static unsigned getHashValue(const CachedHashStrRef& S) {
    return H::GetCHSHashValue(S);
  }
  static bool isEqual(const CachedHashStrRef& LHS,
                      const CachedHashStrRef& RHS) {
    return LHS.hash() == RHS.hash() &&
           DenseMapInfo<StrRef>::isEqual(LHS.val(), RHS.val());
  }
};

/// A container which contains a string, which it owns, plus a precomputed hash.
///
/// We do not null-terminate the string.
class CachedHashString {
  friend struct DenseMapInfo<CachedHashString>;

  static constexpr usize kSSOElts = sizeof(char*);
  using SSOArr = SimpleArray<char, kSSOElts>;

  union {
    char* P;
    SSOArr SSO;
  };
  u32 Size;
  u32 Hash;

  ////////////////////////////////////////////////////////////////////////
  // DenseMapInfo

  static char* GetEmptyKeyPtr() {
    return DenseMapInfo<char*>::getEmptyKey();
  }
  static char* GetTombstoneKeyPtr() {
    return DenseMapInfo<char*>::getTombstoneKey();
  }

  bool isEmptyOrTombstone() const {
    if (Size > 0) return false;
    return P == GetEmptyKeyPtr() || P == GetTombstoneKeyPtr();
  }

  struct ConstructEmptyOrTombstoneTy {};

  CachedHashString(ConstructEmptyOrTombstoneTy, char* EmptyOrTombstonePtr)
      : P(EmptyOrTombstonePtr), Size(0), Hash(0) {
    exi_assert(isEmptyOrTombstone());
  }

  ////////////////////////////////////////////////////////////////////////
  // SSO

  ALWAYS_INLINE char* getZeroPtr() const {
#if EXI_CACHEDHASHSTRING_UNIQUE_ZERO
    // Use a pointer to this object so each instance is unique.
    return (char*)(this);
#else
    return nullptr;
#endif
  }

#if !EXI_CACHEDHASHSTRING_UNIQUE_ZERO
  ALWAYS_INLINE static
#endif
  char* getZeroPtrAlt([[maybe_unused]] CachedHashString& Other) {
#if EXI_CACHEDHASHSTRING_UNIQUE_ZERO
    exi_invariant(!this->isSSO());
    return !isZero() ? this->P : Other.getZeroPtr();
#else
    return nullptr;
#endif
  }

  EXI_INLINE bool isSSO() const {
    return Size > 0 && Size <= kSSOElts;
  }
  EXI_INLINE bool isDynamic() const {
    return Size > kSSOElts;
  }
  EXI_INLINE bool isZero() const {
    return Size == 0 && P == getZeroPtr();
  }

  char* ptr() { return isSSO() ? SSO.Data : P; }
  const char* ptr() const { return isSSO() ? SSO.Data : P; }

  char* allocOrMemsetOnCtor() {
    if (Size == 0) {
      this->P = getZeroPtr();
      return P;
    } else if (isSSO()) {
      this->SSO = SSOArr{};
      return SSO.Data;
    }

    this->P = new char[Size];
    return P;
  }

  inline void handleCtor(const char* Data) {
    char* Ptr = allocOrMemsetOnCtor();
    if (Size != 0)
      std::memcpy(Ptr, Data, Size);
  }

  /// Handles swapping between disparate pointer classes.
  static void SwapPtrAndSSO(CachedHashString& SSOS, CachedHashString& PtrS) {
    exi_invariant(SSOS.isSSO() && !PtrS.isSSO());
    /// In this case, we need to give the object a unique "empty" state.
#if EXI_CACHEDHASHSTRING_UNIQUE_ZERO
    if EXI_UNLIKELY(PtrS.Size == 0 && !PtrS.isEmptyOrTombstone()) {
      PtrS.SSO = SSOS.SSO;
      SSOS.P = SSOS.getZeroPtr();
      return;
    } else
#endif
    /*Otherwise, swapping is simple!*/ {
      char* Ptr = PtrS.P;
      PtrS.SSO = SSOS.SSO;
      SSOS.P = Ptr;
    }
  }

  /// Handles swapping of cached hash string buffers. Must be called before
  /// swapping any other members.
  static void SwapBuffers(CachedHashString& LHS, CachedHashString& RHS) {
    using std::swap;
    if (LHS.isSSO()) {
      if (RHS.isSSO())
        swap(LHS.SSO, RHS.SSO);
      else
        SwapPtrAndSSO(LHS, RHS);
    } else {
      if (RHS.isSSO())
        SwapPtrAndSSO(RHS, LHS);
      else {
#if EXI_CACHEDHASHSTRING_UNIQUE_ZERO
        char* Ptr = LHS.getZeroPtrAlt(RHS);
        LHS.P = RHS.getZeroPtrAlt(LHS);
        RHS.P = Ptr;
#else
        swap(LHS.P, RHS.P);
#endif
      }
    }
  }

public:
  explicit CachedHashString(const char* S) : CachedHashString(StrRef(S)) {}

  // Explicit because copying and hashing a string isn't free.
  explicit CachedHashString(StrRef S)
      : CachedHashString(S, DenseMapInfo<StrRef>::getHashValue(S)) {}

  CachedHashString(StrRef S, u32 Hash)
      : Size(S.size()), Hash(Hash) {
    this->handleCtor(S.data());
  }

  // Ideally this class would not be copyable. But SetVector requires copyable
  // keys, and we want this to be usable there.
  CachedHashString(const CachedHashString& Other)
      : Size(Other.Size), Hash(Other.Hash) {
    if (Other.isEmptyOrTombstone()) {
      P = Other.P;
    } else {
      this->handleCtor(Other.ptr());
    }
  }

  CachedHashString& operator=(CachedHashString Other) {
    // swaps with a temporary value which gets destroyed.
    swap(*this, Other);
    return *this;
  }

  CachedHashString(CachedHashString&& Other) noexcept
      : Size(Other.Size), Hash(Other.Hash) {
    if (Other.isSSO())
      this->SSO = Other.SSO;
    else
      this->P = Other.P;
    // Set Other to empty state.
    Other.P = GetEmptyKeyPtr();
    Other.Size = 0;
    Other.Hash = 0;
  }

  ~CachedHashString() {
    if (isDynamic())
      delete[] P;
  }

  StrRef val() const { return StrRef(ptr(), Size); }
  u32 size() const { return Size; }
  u32 hash() const { return Hash; }

  operator StrRef() const { return val(); }
  operator CachedHashStrRef() const {
    return CachedHashStrRef(val(), Hash);
  }

  friend void swap(CachedHashString& LHS, CachedHashString& RHS) {
    using std::swap;
    SwapBuffers(LHS, RHS);
    swap(LHS.Size, RHS.Size);
    swap(LHS.Hash, RHS.Hash);
  }
};

template <> struct DenseMapInfo<CachedHashString> {
  static CachedHashString getEmptyKey() {
    return CachedHashString(CachedHashString::ConstructEmptyOrTombstoneTy(),
                            CachedHashString::GetEmptyKeyPtr());
  }
  static CachedHashString getTombstoneKey() {
    return CachedHashString(CachedHashString::ConstructEmptyOrTombstoneTy(),
                            CachedHashString::GetTombstoneKeyPtr());
  }
  static unsigned getHashValue(const CachedHashString& S) {
    return H::GetCHSHashValue(S);
  }
  static bool isEqual(const CachedHashString& LHS,
                      const CachedHashString& RHS) {
    if (LHS.hash() != RHS.hash())
      return false;
    if (LHS.P == CachedHashString::GetEmptyKeyPtr())
      return RHS.P == CachedHashString::GetEmptyKeyPtr();
    if (LHS.P == CachedHashString::GetTombstoneKeyPtr())
      return RHS.P == CachedHashString::GetTombstoneKeyPtr();

    // This is safe because if RHS.P is the empty or tombstone key, it will have
    // length 0, so we'll never dereference its pointer.
    return LHS.val() == RHS.val();
  }
};

} // namespace exi
