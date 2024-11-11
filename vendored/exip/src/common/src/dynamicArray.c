/*==================================================================*\
|                EXIP - Embeddable EXI Processor in C                |
|--------------------------------------------------------------------|
|          This work is licensed under BSD 3-Clause License          |
|  The full license terms and conditions are located in LICENSE.txt  |
\===================================================================*/

/**
 * @file dynamicArray.c
 * @brief Implementation for untyped dynamic array
 *
 * @date Jan 25, 2011
 * @author Rumen Kyusakov
 * @version 0.5
 * @par[Revision] $Id$
 */

#include "dynamicArray.h"
#include "memManagement.h"

/**
 * Used for fixing portability issue due to padding in DynArray derived structures such as
 *  struct [ConcreteDynamicArray]
 *   {
 *     DynArray dynArray;
 *     [ArrayEntryType]* base;
 *     Index count;
 *   };
 *   The fix is contributed by John Fisher (see https://sourceforge.net/p/exip/discussion/Q%26A/thread/ac602d44/#ae3c)
 */
typedef struct
{
  DynArray dynArray;
  void* base;
  Index count;
} OuterDynamicArray;

#define OUTER(ex) ((OuterDynamicArray *)(ex))

errorCode createDynArray(DynArray* dynArray, size_t entrySize, uint16_t chunkEntries)
{
	void** base = &OUTER(dynArray)->base;
	Index* count = &OUTER(dynArray)->count;

	*base = EXIP_MALLOC(entrySize*chunkEntries);
	if EXIP_UNLIKELY(*base == NULL)
		return EXIP_MEMORY_ALLOCATION_ERROR;

	dynArray->entrySize = entrySize;
	*count = 0;
	dynArray->chunkEntries = chunkEntries;
	dynArray->arrayEntries = chunkEntries;

	return EXIP_OK;
}

errorCode addEmptyDynEntry(DynArray* dynArray, void** entry, Index* entryID)
{
	if(dynArray == NULL)
		return EXIP_NULL_POINTER_REF;

	void** base = &OUTER(dynArray)->base;
	Index* count = &OUTER(dynArray)->count;
	const Index vcount = *count;

	if EXIP_UNLIKELY(dynArray->arrayEntries == vcount)   // The dynamic array must be extended first
	{
		const size_t addedEntries = (dynArray->chunkEntries == 0) ?
			DEFAULT_NUMBER_CHUNK_ENTRIES : (dynArray->chunkEntries * 3) / 2;

		if (*base == NULL) {
			*base = EXIP_MALLOC(dynArray->entrySize * addedEntries);
			if EXIP_UNLIKELY(*base == NULL)
				return EXIP_MEMORY_ALLOCATION_ERROR;
		} else {
			*base = EXIP_REALLOC(*base, dynArray->entrySize * (vcount + addedEntries));
			if EXIP_UNLIKELY(*base == NULL)
				return EXIP_MEMORY_ALLOCATION_ERROR;
		}

		dynArray->arrayEntries = dynArray->arrayEntries + addedEntries;
	}

	*entry = (unsigned char *)(*base) + (vcount * dynArray->entrySize);
	*entryID = vcount;
	*count += 1;

	return EXIP_OK;
}

errorCode addDynEntry(DynArray* dynArray, void* entry, Index* entryID)
{
	errorCode tmp_err_code;
	void *emptyEntry;

	TRY(addEmptyDynEntry(dynArray, &emptyEntry, entryID));

	memcpy(emptyEntry, entry, dynArray->entrySize);
	return EXIP_OK;
}

errorCode delDynEntry(DynArray* dynArray, Index entryID)
{
	void** base;
	Index* count;

	if(dynArray == NULL)
		return EXIP_NULL_POINTER_REF;

	base = &OUTER(dynArray)->base;
	count = &OUTER(dynArray)->count;

	const Index arrCount = *count - 1;
	if(entryID == arrCount) {
		*count -= 1;
	} else if((int64_t)(arrCount - entryID) >= 0) {
		unsigned char* const rawBase = (unsigned char *)(*base);
		unsigned char* const offset = rawBase + (entryID * dynArray->entrySize);
		/* Shuffle the array down to fill the removed entry */
		memcpy(offset, offset + dynArray->entrySize,
			   (arrCount - entryID) * dynArray->entrySize);
		*count -= 1;
	}
	else
		return EXIP_OUT_OF_BOUND_BUFFER;

	return EXIP_OK;
}

void destroyDynArray(DynArray* dynArray)
{
	void** base = &OUTER(dynArray)->base;
	EXIP_MFREE(*base);
}
