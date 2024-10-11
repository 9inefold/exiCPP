//===- new/hashtable_private2.h -------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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

#ifndef EXIP_HASHTABLE_PRIVATE_H_
#define EXIP_HASHTABLE_PRIVATE_H_

#include "hashtable.h"

/// Implemented so that keys are stored after values.
struct hashentry /* HashEntry */ {
  size_t keyLength;
  HashEntryValue value;
  const CharType* key;
};

struct hashtable /* HashTable */ {
  HashEntry** table;
  unsigned bucketCount;
  unsigned itemCount;
  unsigned tombstoneCount;
  // unsigned itemSize = sizeof(HashEntry);
};

enum hashtable_constants {
  hashentry_size  = sizeof(HashEntry),
  hashentry_align = 8,
  hashentry_sentinel = 2,
  hashtable_itemSize = sizeof(HashEntry),
};

enum tombstone_traits {
#if EXIP_USE_MIMALLOC
  /// Same as `log2(8)`.
  /// Assumes mi_malloc is at least 8-byte aligned.
  tombstone_lowBits = 3ULL,
#else
  /// Same as `log2(4)`.
  /// Assumes malloc is at least 4-byte aligned.
  tombstone_lowBits = 2ULL,
#endif
};

//////////////////////////////////////////////////////////////////////////

/// @brief Allocates and initializes new `HashEntry`.
/// @param key The string to use.
HashEntry* hashentry_allocateWithKey(String key);

/// @brief Initializes an allocated `HashTable`.
void hashtable_init(HashTable* thiz, unsigned initSize);

unsigned hashtable_rehashWith(HashTable* thiz, unsigned bucketKey);
static inline unsigned hashtable_rehash(HashTable* thiz) {
  return hashtable_rehashWith(thiz, 0U);
}

unsigned hashtable_lookupBucketForWith(HashTable* thiz, String key, HashValue fullHash);

static inline unsigned hashtable_lookupBucketFor(HashTable* thiz, String key) {
  return hashtable_lookupBucketForWith(thiz, key, hashtable_hash(key));
}

int hashtable_findKeyWith(HashTable* thiz, String key, HashValue fullHash);

static inline int hashtable_findKey(HashTable* thiz, String key) {
  return hashtable_findKeyWith(thiz, key, hashtable_hash(key));
}

void hashtable_removeKeyEx(HashTable* thiz, HashEntry* entry);
HashEntry* hashtable_removeKey(HashTable* thiz, String key);

#endif // EXIP_HASHTABLE_PRIVATE_H_
