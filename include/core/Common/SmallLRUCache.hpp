//===- Common/SmallLRUCache.hpp -------------------------------------===//
//
// Copyright (C) 2024 Ninefold
//
// Licensed under the Apache License, Version 2.0 (the "License");
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
/// This file defines a cache with N inline elements.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Array.hpp>
#include <Common/CompressedPair.hpp>
// #include <Common/ManualDrop.hpp>
#include <Common/Option.hpp>
#include <Common/function_ref.hpp>
#include <Support/D/MemOps.hpp>

// TODO: Move EXI_USE_FAST_LRUCACHE_SHIFTING
#define EXI_USE_FAST_LRUCACHE_SHIFTING 1

namespace exi {

template <typename K, typename V>
struct LRUCacheInfo {
  static constexpr bool isKeyNull(const K&) { return false; }
  static constexpr K getKey(const K& Key) { return Key; }
  static constexpr V getValue(const K&) { return V(); }
};

template <typename K, typename V, usize N,
  class InfoT = LRUCacheInfo<K, V>>
class SmallLRUCache {
  static_assert(N > 0, "Zero element caches are not allowed.");

  using KVType = CompressedPair<K, V>;
  static constexpr usize LastElt = (N - 1);

  /// Inline storage for the elements.
  Array<KVType, N> Elts;
  /// Current size of the inline storage.
  usize Size = 0;

  ALWAYS_INLINE KVType GetNewElt(const K& Key) {
    return KVType {
      InfoT::getKey(Key),
      InfoT::getValue(Key)
    };
  }
  
  /// Handles the shifting of old elements.
  inline void shiftFrom(usize Ix = 0) {
    const usize MRU = (Size - 1);
#if EXI_USE_FAST_LRUCACHE_SHIFTING
    // TODO: Add LRUCache tests
    exi_invariant(Ix < MRU, "Index out of range!");
    auto* Begin = Elts.data() + Ix;
    exi::FastInitMove(Begin, Begin + 1, MRU - Ix);
#else
    for (; Ix < MRU; ++Ix) {
      // Move element back one position.
      Elts[Ix] = std::move(Elts[Ix + 1]);
    }
#endif
  }

public:
  Option<V&> get(const K& Key) {
    if EXI_NEVER(InfoT::isKeyNull(Key))
      return std::nullopt;

    for (usize Ix = Size; Ix-- > 0;) {
      if (Elts[Ix].Key == Key) {
        const usize MRU = Size - 1;
        if (Ix < MRU) {
          KVType Entry = std::move(Elts[Ix]);
          this->shiftFrom(Ix);
          Elts[MRU] = std::move(Entry);
        }
        return Elts[MRU].Value;
      }
    }

    if (Size == N) {
      // Shift all elements back if cache is full.
      this->shiftFrom(0);
    } else {
      // Otherwise add a new element.
      ++Size;
    }

    Elts[Size - 1] = GetNewElt(Key);
    return Elts[Size - 1].Value;
  }

  inline bool set(const K& Key, auto&& Val) {
    Option<V&> Out = get(Key);
    if EXI_NEVER(Out.is_none())
      return false;
    *Out = EXI_FWD(Val);
    return true;
  }

  inline bool remove(const K& Key) {
    if EXI_NEVER(InfoT::isKeyNull(Key))
      return false;
    for (usize Ix = Size; Ix-- > 0;) {
      if (Elts[Ix].Key == Key) {
        const usize MRU = Size - 1;
        if (Ix < MRU)
          this->shiftFrom(Ix);
        --Size;
        return true;
      }
    }
    return false;
  }

  /// Runs an update function on all active values.
  void update(function_ref<void(const K, V&)> Fn) {
    for (usize Ix = 0; Ix < Size; ++Ix)
      Fn(Elts[Ix].Key, Elts[Ix].Value);
  }

  ALWAYS_INLINE usize size() const { return Size; }
  ALWAYS_INLINE bool empty() const { return Size == 0; }
};

} // namespace exi
