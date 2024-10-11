//===- new/hashtable.h ----------------------------------------------===//
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
//
//  This file provides a specialized hashmap interface based on
//  LLVM's StringMap. Uses rapidhash for hashing by default.
//
//===----------------------------------------------------------------===//

#ifndef EXIP_HASHTABLE_H_
#define EXIP_HASHTABLE_H_

#include "config.h"
#include "procTypes.h"

#define EXTRA_CHECKS 1
#if EXTRA_CHECKS
# define HASHMAP_ASSERT(ex) assert(ex)
#else
# define HASHMAP_ASSERT(ex) ((void)(0))
#endif

typedef struct hashentry HashEntry;
typedef struct hashtable HashTable;

/// The type of the stored hashes.
typedef uint32_t HashValue;

/// The type of hashentry values.
typedef Index HashEntryValue;

/// Uses rapidhash (successor to wyhash) to hash strings.
HashValue hashtable_hash(String key);

//======================================================================//
// HashEntry
//======================================================================//

static inline size_t hashentry_getKeyLength(const HashEntry* thiz) {
  HASHMAP_ASSERT(thiz != NULL);
  // Get pointer to first element.
  const size_t* const ptr = (const size_t*)(thiz);
  return *ptr;
}

/// @brief Gets the raw key of a `HashEntry`.
/// @return Immutable pointer to the key.
const CharType* hashentry_getKeyData(const HashEntry* thiz);

/// @brief Gets the value of a `HashEntry` key.
/// @return Pointer to the entry.
HashEntry* hashentry_getFromKeyData(const CharType* keyData);

/// @brief Gets the wrapped key of a `HashEntry`.
/// @return Wrapper around the key.
/// @note Not allowed to modify the table content from this value.
static inline String hashentry_getKey(const HashEntry* thiz) {
  const String key = {
    (CharType*)hashentry_getKeyData(thiz),
    hashentry_getKeyLength(thiz)
  };
  return key;
}

HashEntryValue* hashentry_getValue(HashEntry* thiz);
void hashentry_setValue(HashEntry* thiz, HashEntryValue val);

HashEntry* hashentry_create(String key, HashEntryValue val);
static inline void hashentry_destroy(HashEntry* thiz) {
  HASHMAP_ASSERT(thiz != NULL);
  EXIP_MFREE(thiz);
}

//======================================================================//
// HashTable
//======================================================================//

#define create_hashtable(initSize, ...) hashtable_create((initSize))

/// @brief Allocates and initialized a `HashTable`.
/// @param initSize The minimum number of buckets
/// @return An initialized `HashTable*`.
HashTable* hashtable_create(unsigned initSize);

/// @brief Deletes entries and frees table. 
void hashtable_destroy(HashTable* thiz);

/// @brief Destroys table and frees memory. 
static inline void hashtable_destroyAndRelease(HashTable** pthiz) {
  HASHMAP_ASSERT(pthiz != NULL);
  HashTable* const thiz = *pthiz;
  if EXIP_UNLIKELY(thiz == NULL)
    return;

  hashtable_destroy(thiz);
  EXIP_MFREE(thiz);
  *pthiz = NULL;
}

//////////////////////////////////////////////////////////////////////////

errorCode hashtable_insert(HashTable* thiz, String key, HashEntryValue value);

HashEntryValue hashtable_search(HashTable* thiz, String key);

HashEntryValue hashtable_remove(HashTable* thiz, String key);

unsigned hashtable_count(HashTable* thiz);

#endif // EXIP_HASHTABLE_H_
