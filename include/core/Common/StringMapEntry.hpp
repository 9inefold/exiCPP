//===- Common/StringMapEntry.hpp ------------------------------------===//
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
/// This file defines the StringMapEntry class - it is intended to be a low
/// dependency implementation detail of StringMap that is more suitable for
/// inclusion in public headers than StringMap.h itself is.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Option.hpp>
#include <Common/StrRef.hpp>
#include <utility>

namespace exi {

/// StringMapEntryBase - Shared base class of StringMapEntry instances.
class StringMapEntryBase {
  usize keyLength;

public:
  explicit StringMapEntryBase(usize keyLength) : keyLength(keyLength) {}

  usize getKeyLength() const { return keyLength; }

protected:
  /// Helper to tail-allocate \p Key. It'd be nice to generalize this so it
  /// could be reused elsewhere, maybe even taking an llvm::function_ref to
  /// type-erase the allocator and put it in a source file.
  template <typename AllocatorTy>
  static void *allocateWithKey(usize EntrySize, usize EntryAlign,
                               StrRef Key, AllocatorTy &Allocator);
};

// Define out-of-line to dissuade inlining.
template <typename AllocatorTy>
void *StringMapEntryBase::allocateWithKey(usize EntrySize, usize EntryAlign,
                                          StrRef Key,
                                          AllocatorTy &Allocator) {
  usize KeyLength = Key.size();

  // Allocate a new item with space for the string at the end and a null
  // terminator.
  usize AllocSize = EntrySize + KeyLength + 1;
  void *Allocation = Allocator.Allocate(AllocSize, EntryAlign);
  exi_assert(Allocation, "Unhandled out-of-memory");

  // Copy the string information.
  char *Buffer = reinterpret_cast<char *>(Allocation) + EntrySize;
  if (KeyLength > 0)
    ::memcpy(Buffer, Key.data(), KeyLength);
  Buffer[KeyLength] = 0; // Null terminate for convenience of clients.
  return Allocation;
}

/// StringMapEntryStorage - Holds the value in a StringMapEntry.
///
/// Factored out into a separate base class to make it easier to specialize.
/// This is primarily intended to support StringSet, which doesn't need a value
/// stored at all.
template <typename ValueTy>
class StringMapEntryStorage : public StringMapEntryBase {
public:
  ValueTy second;

  explicit StringMapEntryStorage(usize keyLength)
      : StringMapEntryBase(keyLength), second() {}
  template <typename... InitTy>
  StringMapEntryStorage(usize keyLength, InitTy &&...initVals)
      : StringMapEntryBase(keyLength),
        second(std::forward<InitTy>(initVals)...) {}
  StringMapEntryStorage(StringMapEntryStorage &e) = delete;

  const ValueTy &getValue() const { return second; }
  ValueTy &getValue() { return second; }

  void setValue(const ValueTy &V) { second = V; }
};

template <>
class StringMapEntryStorage<nullopt_t> : public StringMapEntryBase {
public:
  explicit StringMapEntryStorage(usize keyLength,
                                 nullopt_t = std::nullopt)
      : StringMapEntryBase(keyLength) {}
  StringMapEntryStorage(StringMapEntryStorage &entry) = delete;

  nullopt_t getValue() const { return std::nullopt; }
};

/// StringMapEntry - This is used to represent one value that is inserted into
/// a StringMap.  It contains the Value itself and the key: the string length
/// and data.
template <typename ValueTy>
class StringMapEntry final : public StringMapEntryStorage<ValueTy> {
public:
  using StringMapEntryStorage<ValueTy>::StringMapEntryStorage;

  using ValueType = ValueTy;

  StrRef getKey() const {
    return StrRef(getKeyData(), this->getKeyLength());
  }

  /// getKeyData - Return the start of the string data that is the key for this
  /// value.  The string data is always stored immediately after the
  /// StringMapEntry object.
  const char *getKeyData() const {
    return reinterpret_cast<const char *>(this + 1);
  }

  StrRef first() const { return getKey(); }

  /// Create a StringMapEntry for the specified key construct the value using
  /// \p InitiVals.
  template <typename AllocatorTy, typename... InitTy>
  static StringMapEntry *create(StrRef key, AllocatorTy &allocator,
                                InitTy &&...initVals) {
    return new (StringMapEntryBase::allocateWithKey(
        sizeof(StringMapEntry), alignof(StringMapEntry), key, allocator))
        StringMapEntry(key.size(), std::forward<InitTy>(initVals)...);
  }

  /// GetStringMapEntryFromKeyData - Given key data that is known to be embedded
  /// into a StringMapEntry, return the StringMapEntry itself.
  static StringMapEntry &GetStringMapEntryFromKeyData(const char *keyData) {
    char *ptr = const_cast<char *>(keyData) - sizeof(StringMapEntry<ValueTy>);
    return *reinterpret_cast<StringMapEntry *>(ptr);
  }

  /// Destroy - Destroy this StringMapEntry, releasing memory back to the
  /// specified allocator.
  template <typename AllocatorTy> void Destroy(AllocatorTy &allocator) {
    // Free memory referenced by the item.
    usize AllocSize = sizeof(StringMapEntry) + this->getKeyLength() + 1;
    this->~StringMapEntry();
    allocator.Deallocate(static_cast<void *>(this), AllocSize,
                         alignof(StringMapEntry));
  }
};

// Allow structured bindings on StringMapEntry.

template <usize Index, typename ValueTy>
decltype(auto) get(StringMapEntry<ValueTy> &E) {
  static_assert(Index < 2);
  if constexpr (Index == 0)
    return E.getKey();
  else
    return E.getValue();
}

template <usize Index, typename ValueTy>
decltype(auto) get(const StringMapEntry<ValueTy> &E) {
  static_assert(Index < 2);
  if constexpr (Index == 0)
    return E.getKey();
  else
    return E.getValue();
}

} // namespace exi

template <typename ValueTy>
struct std::tuple_size<exi::StringMapEntry<ValueTy>>
    : std::integral_constant<usize, 2> {};

template <usize Index, typename ValueTy>
struct std::tuple_element<Index, exi::StringMapEntry<ValueTy>>
    : std::tuple_element<Index, std::pair<exi::StrRef, ValueTy>> {};
