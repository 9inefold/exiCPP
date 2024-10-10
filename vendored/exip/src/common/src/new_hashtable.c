//===- hashtable.c --------------------------------------------------===//
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

#include "new/hashtable.h"
#include "new/hashtable_private.h"
#include "procTypes.h"
#include "stringManipulate.h"
#include "rapidhash.h"

#if !defined(EXIP_MALLOC) || !defined(EXIP_CALLOC) || !defined(EXIP_MFREE)
# error exipConfig.h must be included!
#endif

#if defined(__GNUC__)
# define getEntryBuffer(entry) ((entry)->key)
#else
# define getEntryBuffer(entry) ((CharType*)((entry) + 1))
#endif

static inline void* internalMalloc(size_t len) {
#if EXIP_USE_MIMALLOC
  return mi_malloc_aligned(len, 8);
#else
  return EXIP_MALLOC(len);
#endif
}

static inline void* internalCalloc(size_t count, size_t size) {
#if EXIP_USE_MIMALLOC
  return mi_calloc_aligned(count, size, 8);
#else
  return EXIP_CALLOC(count, size);
#endif
}

static inline uint64_t nextPowerOf2(uint64_t V) {
  V |= (V >> 1);
  V |= (V >> 2);
  V |= (V >> 4);
  V |= (V >> 8);
  V |= (V >> 16);
  V |= (V >> 32);
  return V + 1;
}

static inline unsigned getMinimumBucketsFor(unsigned entryCount) {
  // Ensure that "nEntries * 4 < nEntries * 3"
  if (entryCount == 0)
    return 0;
  const unsigned x4 = entryCount * 4;
  return nextPowerOf2(x4 / 3 + 1);
}

static inline HashEntry** createEntryTable(unsigned bucketCount) {
  const size_t entrySize = sizeof(HashEntry**) + sizeof(unsigned);
  HashEntry** table = internalCalloc(bucketCount + 1, entrySize);
  assert(table != NULL && "Out of memory!");

  // Allocate extra sentinel bucket.
  table[bucketCount] = (HashEntry*)hashentry_sentinel;
  return table;
}

static inline unsigned* getHashTable(HashEntry** table, unsigned bucketCount) {
  return (unsigned*)(table + bucketCount + 1);
}

static inline HashEntry* getTombstonePtr(void) {
  return (HashEntry*)((~0ULL) << tombstone_lowBits);
}

//////////////////////////////////////////////////////////////////////////

HashEntry* hashentry_allocateWithKey(String key) {
  const size_t keyLength = key.length;
  const size_t keyLengthBytes = keyLength * sizeof(CharType);

  // Allocate storage for [entry][keystring][\0].
  const size_t allocSize = hashentry_size + keyLengthBytes + 1;
  HashEntry* allocation = internalMalloc(allocSize);
  assert(allocation != NULL && "Out of memory!");

  CharType* buf = getEntryBuffer(allocation);
  if (keyLength > 0)
    memcpy(buf, key.str, keyLengthBytes);
  buf[keyLength] = 0;
  return allocation;
}

uint32_t hashtable_hash(String key) {
  const size_t keyLength = key.length * sizeof(CharType);
  return rapidhash(key.str, keyLength);
}

void hashtable_init(HashTable* thiz, unsigned initSize) {
  assert(thiz != NULL);
  assert((InitSize & (InitSize - 1)) == 0 &&
    "initSize must be 2^N or zero!");
  
  unsigned bucketCount = initSize ? initSize : 16;
  thiz->table = createEntryTable(bucketCount);
  thiz->bucketCount = bucketCount;
  thiz->itemCount = 0;
  thiz->tombstoneCount = 0;
}

unsigned hashtable_lookupBucketForWith(HashTable* thiz, String key, uint32_t fullHash) {
  HASHMAP_ASSERT(fullHash == hashtable_hash(key));
  if (thiz->bucketCount == 0)
    hashtable_init(thiz, 16);
  unsigned bucketKey = fullHash & (thiz->bucketCount - 1);
  unsigned* hashTable = getHashTable(thiz->table, thiz->bucketCount);

  unsigned probeAmount = 1;
  int firstTombstone = -1;
  while (true) {
    HashEntry* bucketVal = thiz->table[bucketKey];
    // Found an empty bucket, return it.
    if EXIP_LIKELY(!bucketVal) {
      // Reuse tombstone if found.
      if (firstTombstone != -1) {
        hashTable[firstTombstone] = fullHash;
        return firstTombstone;
      }

      hashTable[bucketKey] = fullHash;
      return bucketKey;
    }

    if (bucketVal == getTombstonePtr()) {
      if (firstTombstone == -1)
        firstTombstone = bucketKey;
    } else if EXIP_LIKELY(hashTable[bucketKey] == fullHash) {
      // If hash matches, compare the string values.
      const String valKey = hashentry_getKey(bucketVal);
      if (stringEqual(key, valKey))
        return bucketKey;
    }

    // Didn't find the item, so probe next bucket
    bucketKey = (bucketKey + probeAmount) & (thiz->bucketCount - 1);
    ++probeAmount;
  }
}

int hashtable_findKeyWith(HashTable* thiz, String key, uint32_t fullHash) {
  if (thiz->bucketCount == 0)
    return -1;
  HASHMAP_ASSERT(fullHash == hashtable_hash(key));
  unsigned bucketKey = fullHash & (thiz->bucketCount - 1);
  unsigned* hashTable = getHashTable(thiz->table, thiz->bucketCount);

  unsigned probeAmount = 1;
  while (true) {
    HashEntry* bucketVal = thiz->table[bucketKey];
    // Found an empty bucket, return it.
    if EXIP_LIKELY(!bucketVal)
      return -1;
    
    if (bucketVal == getTombstonePtr()) {
      // Ignore tombstones
    } else if EXIP_LIKELY(hashTable[bucketKey] == fullHash) {
      // If hash matches, compare the string values.
      const String valKey = hashentry_getKey(bucketVal);
      if (stringEqual(key, valKey))
        return bucketKey;
    }

    // Didn't find the item, so probe next bucket
    bucketKey = (bucketKey + probeAmount) & (thiz->bucketCount - 1);
    ++probeAmount;
  }
}

void hashtable_removeKeyEx(HashTable* thiz, HashEntry* entry) {
  const String entryKey = hashentry_getKey(entry);
  HashEntry* entryF = hashtable_removeKey(thiz, entryKey);
  (void)entryF;
  assert(entry == entryF && "Didn't find key!");
}

HashEntry* hashtable_removeKey(HashTable* thiz, String key) {
  int bucket = hashtable_findKey(thiz, key);
  if (bucket == -1)
    return NULL;

  HashEntry* res = thiz->table[bucket];
  thiz->table[bucket] = getTombstonePtr();
  --thiz->itemCount;
  ++thiz->tombstoneCount;
  assert(thiz->itemCount + thiz->tombstoneCount <= thiz->bucketCount);

  return res;
}

unsigned hashtable_rehashWith(HashTable* thiz, const unsigned bucketKey) {
  const unsigned nBuckets = thiz->bucketCount;
  const unsigned nItems = thiz->itemCount;
  unsigned newSize;

  if EXIP_UNLIKELY((nItems * 4) > (nBuckets * 3)) {
    newSize = nBuckets * 2;
  } else if EXIP_UNLIKELY(
    nBuckets - (nItems + thiz->tombstoneCount)
      <= (nBuckets / 8)) {
    newSize = nBuckets;
  } else {
    return bucketKey;
  }

  unsigned newBucketKey   = bucketKey;
  HashEntry** newTable    = createEntryTable(newSize);
  unsigned* hashTable     = getHashTable(thiz->table, thiz->bucketCount);
  unsigned* newHashTable  = getHashTable(newTable, newSize);

  // Rehash all items into their new buckets.
  for (unsigned Ix = 0; Ix != nBuckets; ++Ix) {
    HashEntry* bucketVal = thiz->table[Ix];
    if (!bucketVal || bucketVal == getTombstonePtr())
      continue;

    const unsigned mask = newSize - 1;
    // If the bucket is not available, probe for a spot.
    unsigned fullHash = hashTable[Ix];
    unsigned newBucket = fullHash & mask;
    if (newTable[newBucket]) {
      unsigned probeSize = 1;
      do {
        newBucket = (newBucket + probeSize++) & mask;
      } while (newTable[newBucket]);
    }

    // Found a slot, fill it in.
    newTable[newBucket] = bucketVal;
    newHashTable[newBucket] = fullHash;
    if (Ix == bucketKey)
      newBucketKey = newBucket;
  }

  EXIP_MFREE(thiz->table);

  thiz->table = newTable;
  thiz->bucketCount = newSize;
  thiz->tombstoneCount = 0;

  return newBucketKey;
}

//======================================================================//
// Public Functions
//======================================================================//

const CharType* hashentry_getKeyData(const HashEntry* thiz) {
  return getEntryBuffer(thiz);
}

HashEntryValue* hashentry_getValue(HashEntry* thiz) {
  assert(thiz != NULL);
  return &thiz->value;
}

void hashentry_setValue(HashEntry* thiz, HashEntryValue val) {
  assert(thiz != NULL);
  thiz->value = val;
}

HashEntry* hashentry_create(String key, HashEntryValue val) {
  HashEntry* entry = hashentry_allocateWithKey(key);
  entry->keyLength = key.length;
  entry->value = val;
  return entry;
}

HashEntry* hashentry_getFromKeyData(const CharType* keyData) {
  char* const rawEntry = (char*)(keyData) - hashentry_size;
  return (HashEntry*)(rawEntry);
}

//////////////////////////////////////////////////////////////////////////

HashTable* hashtable_create(unsigned initSize) {
  if (initSize > MAX_HASH_TABLE_SIZE)
    return NULL;
  
  HashTable* tbl = internalMalloc(sizeof(HashTable));
  assert(tbl != NULL && "Out of memory!");

  if (initSize != 0) {
    const unsigned size = getMinimumBucketsFor(initSize);
    hashtable_init(tbl, size);
    return tbl;
  }

  tbl->table = NULL;
  tbl->bucketCount = 0;
  tbl->itemCount = 0;
  tbl->tombstoneCount = 0;
  return tbl;
}

errorCode hashtable_insert(HashTable* thiz, String key, HashEntryValue value) {
  const uint32_t fullHash = hashtable_hash(key);
  unsigned bucketKey = hashtable_lookupBucketForWith(thiz, key, fullHash);
  HashEntry** bucketRef = &thiz->table[bucketKey];
  HashEntry* bucket = *bucketRef;

  if (bucket && bucket != getTombstonePtr()) {
    // TODO: Change this to EXIP_OK?
    return EXIP_HASH_TABLE_ERROR;
  }

  if (bucket == getTombstonePtr())
    --thiz->tombstoneCount;
  *bucketRef = hashentry_create(key, value);
  ++thiz->itemCount;
  assert(thiz->itemCount + thiz->tombstoneCount <= thiz->bucketCount);

  // TODO: Do something with this?
  bucketKey = hashtable_rehashWith(thiz, bucketKey);
  return EXIP_OK;
}

HashEntryValue hashtable_search(HashTable* thiz, String key) {
  const int bucket = hashtable_findKey(thiz, key);
  if (bucket == -1)
    return INDEX_MAX;
  return thiz->table[bucket]->value;
}

HashEntryValue hashtable_remove(HashTable* thiz, String key) {
  HashEntry* entry = hashtable_removeKey(thiz, key);
  if (entry == NULL)
    return INDEX_MAX;
  return entry->value;
}

unsigned hashtable_count(HashTable* thiz) {
  return thiz->itemCount;
}

void hashtable_destroy(HashTable* thiz) {
  if (thiz->itemCount != 0) {
    const unsigned bucketCount = thiz->bucketCount;
    for (unsigned Ix = 0; Ix != bucketCount; ++Ix) {
      HashEntry* bucket = thiz->table[Ix];
      if (bucket && bucket != getTombstonePtr()) {
        hashentry_destroy(bucket);
      }
    }
  }

  if (thiz->table != NULL)
    EXIP_MFREE(thiz->table);
}
