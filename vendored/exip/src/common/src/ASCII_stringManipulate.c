/*==================================================================*\
|                EXIP - Embeddable EXI Processor in C                |
|--------------------------------------------------------------------|
|          This work is licensed under BSD 3-Clause License          |
|  The full license terms and conditions are located in LICENSE.txt  |
\===================================================================*/

/**
 * @file ASCII_stringManipulate.c
 * @brief String manipulation functions used for UCS <-> ASCII transformations
 *
 * @date Sep 3, 2010
 * @author Rumen Kyusakov
 * @version 0.5
 * @par[Revision] $Id$
 */

#include "stringManipulate.h"
#include "memManagement.h"
#include "errorHandle.h"

#ifdef _MSC_VER
# include <intrin.h>
#endif

#define PARSING_STRING_MAX_LENGTH 100

errorCode allocateStringMemory(CharType** str, Index UCSchars)
{
	*str = EXIP_MALLOC(sizeof(CharType)*UCSchars);
	if((*str) == NULL)
		return EXIP_MEMORY_ALLOCATION_ERROR;
	return EXIP_OK;
}

errorCode allocateStringMemoryManaged(CharType** str, Index UCSchars, AllocList* memList)
{
	(*str) = (CharType*) memManagedAllocate(memList, sizeof(CharType)*UCSchars);
	if((*str) == NULL)
		return EXIP_MEMORY_ALLOCATION_ERROR;
	return EXIP_OK;
}

/**
 * Simple translation working only for ASCII characters
 */
errorCode writeCharToString(String* str, uint32_t code_point, Index* writerPosition)
{
	if(*writerPosition >= str->length)
		return EXIP_OUT_OF_BOUND_BUFFER;
	str->str[*writerPosition] = (CharType) code_point;
	*writerPosition += 1;
	return EXIP_OK;
}

void getEmptyString(String* emptyStr)
{
	emptyStr->length = 0;
	emptyStr->str = NULL;
}

boolean isStringEmpty(const String* str)
{
	if(str == NULL || str->length == 0)
		return 1;
	return 0;
}

errorCode asciiToStringN(const char* inStr, Index len, String* outStr, AllocList* memList, boolean clone)
{
	outStr->length = len;
	if(outStr->length > 0)  // If == 0 -> empty string
	{
		if(clone == FALSE)
		{
			outStr->str = (CharType*) inStr;
			return EXIP_OK;
		}
		else
		{
			outStr->str = (CharType*) memManagedAllocate(memList, sizeof(CharType)*(outStr->length));
			if(outStr->str == NULL)
				return EXIP_MEMORY_ALLOCATION_ERROR;
			memcpy(outStr->str, inStr, outStr->length);
			return EXIP_OK;
		}
	}
	else
		outStr->str = NULL;
	return EXIP_OK;
}


errorCode asciiToString(const char* inStr, String* outStr, AllocList* memList, boolean clone)
{
	return asciiToStringN(inStr, strlen(inStr), outStr, memList, clone);
}

boolean stringEqual(const String str1, const String str2)
{
	if(str1.length != str2.length)
		return 0;
	else if(str1.length == 0)
		return 1;
	else // The strings have the same length
	{
		Index i;
		for(i = 0; i < str1.length; i++)
		{
			if(str1.str[i] != str2.str[i])
				return 0;
		}
		return 1;
	}
}

boolean stringEqualToAscii(const String str1, const char* str2)
{
	if(str1.length != strlen(str2))
		return 0;
	else // The strings have the same length
	{
		Index i;
		for(i = 0; i < str1.length; i++)
		{
			if(str1.str[i] != str2[i])
				return 0;
		}
		return 1;
	}
}

int stringCompare(const String str1, const String str2)
{
	/* Check for NULL string pointers */
	if(str1.str == NULL)
	{
		if(str2.str == NULL)
			return 0;
		return -1;
	}
	else if(str2.str == NULL)
		return 1;
	else // None of the strings is NULL
	{
		int diff;
		Index i;
		for(i = 0; i < str1.length && i < str2.length; i++)
		{
			diff = str1.str[i] - str2.str[i];
			if(diff)
				return diff; // There is a difference in the characters at index i
		}
		/* Up to index i the strings have the same characters and might differ only in length*/
		return str1.length - str2.length;
	}
}

uint32_t readCharFromString(const String* str, Index* readerPosition)
{
	assert(*readerPosition < str->length);
	*readerPosition += 1;
	return (uint32_t) str->str[*readerPosition - 1];
}

errorCode cloneString(const String* src, String* newStr)
{
	if(newStr == NULL)
		return EXIP_NULL_POINTER_REF;
	newStr->str = EXIP_MALLOC(sizeof(CharType)*src->length);
	if(newStr->str == NULL)
		return EXIP_MEMORY_ALLOCATION_ERROR;
	newStr->length = src->length;
	memcpy(newStr->str, src->str, src->length);
	return EXIP_OK;
}

errorCode cloneStringManaged(const String* src, String* newStr, AllocList* memList)
{
	if(newStr == NULL)
		return EXIP_NULL_POINTER_REF;
	newStr->str = memManagedAllocate(memList, sizeof(CharType)*src->length);
	if(newStr->str == NULL)
		return EXIP_MEMORY_ALLOCATION_ERROR;
	newStr->length = src->length;
	memcpy(newStr->str, src->str, src->length);
	return EXIP_OK;
}

Index getIndexOfChar(const String* src, CharType sCh)
{
	Index i;

	for(i = 0; i < src->length; i++)
	{
		if(src->str[i] == sCh)
			return i;
	}
	return INDEX_MAX;
}

errorCode stringToInteger(const String* src, int* number)
{
	char buff[PARSING_STRING_MAX_LENGTH];
	long result;
	char *endPointer;

	if(src->length == 0 || src->length >= PARSING_STRING_MAX_LENGTH)
		return EXIP_INVALID_STRING_OPERATION;

	memcpy(buff, src->str, src->length);
	buff[src->length] = '\0';

	result = strtol(buff, &endPointer, 10);

	if(result == LONG_MAX || result == LONG_MIN || *src->str == *endPointer)
		return EXIP_INVALID_STRING_OPERATION;

	if(result >= INT_MAX || result <= INT_MIN)
		return EXIP_OUT_OF_BOUND_BUFFER;

	*number = (int) result;

	return EXIP_OK;
}

errorCode stringToInt64(const String* src, int64_t* number)
{
	char buff[PARSING_STRING_MAX_LENGTH];
	long long result;
	char *endPointer;

	if(src->length == 0 || src->length >= PARSING_STRING_MAX_LENGTH)
		return EXIP_INVALID_STRING_OPERATION;

	memcpy(buff, src->str, src->length);
	buff[src->length] = '\0';

	result = EXIP_STRTOLL(buff, &endPointer, 10);

	if(result == LLONG_MAX || result == LLONG_MIN || *src->str == *endPointer)
		return EXIP_INVALID_STRING_OPERATION;

	if(result >= LLONG_MAX || result <= LLONG_MIN)
		return EXIP_OUT_OF_BOUND_BUFFER;

	*number = (int64_t) result;

	return EXIP_OK;
}

//======================================================================//
// Data -> String
//======================================================================//

#if EXIP_IMPLICIT_DATA_TYPE_CONVERSION

#ifndef _MSC_VER
# ifdef __has_builtin
# if __has_builtin(__builtin_clzll)
#  define EXIP_CLZLL(x) __builtin_clzll(x)
# endif
# endif // __has_builtin
#else // _MSC_VER

# ifndef __clang__
#  pragma intrinsic(_BitScanForward)
#  pragma intrinsic(_BitScanReverse)
#  ifdef _WIN64
#   pragma intrinsic(_BitScanForward64)
#   pragma intrinsic(_BitScanReverse64)
#  endif // _Win64
# endif // __clang__

static inline int msc_clzll(Integer V) {
  assert(V != 0 && "Invalid clzll input!");
  unsigned long Out = 0;
#ifdef _WIN64
  _BitScanReverse64(&Out, V);
#else // _WIN64
  if (_BitScanReverse(&Out, uint32_t(V >> 32)))
    return 63 - int(Out + 32);
  _BitScanReverse(&Out, uint32_t(V));
#endif // _WIN64
  return 63 - int(Out);
}

# define EXIP_CLZLL(x) ::msc_clzll(x)

#endif // _MSC_VER

#ifdef EXIP_CLZLL
static const Integer LogTable10[] = {
  1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4,
  5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 
  10, 10, 10, 10, 11, 11, 11, 12, 12, 12,
  13, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16, 16, 
  17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20
};
static const Integer ZeroOrPow10[] = {
  0, 0, 10, 
  100, 1000,
  10000, 100000, 
  1000000, 10000000, 
  100000000, 1000000000, 
  10000000000, 100000000000, 
  1000000000000, 10000000000000, 
  100000000000000, 1000000000000000, 
  10000000000000000, 100000000000000000, 
  1000000000000000000, 10000000000000000000ULL
};

static inline int clzll(Index V) {
  const int Sub = (sizeof(unsigned long long) * 8) - 64;
  return EXIP_CLZLL(V) - Sub;
}

static inline int intLog2(Index V) {
  return 63 - clzll(V | 1);
}
#endif

static inline int countBase10(Index V) {
#ifdef EXIP_CLZLL
  const int Out = LogTable10[intLog2(V)];
  return Out - (V < ZeroOrPow10[Out]);
#else
  int Total = 1;
  while (true) {
    if (V < 10) 	 return Total;
    if (V < 100) 	 return Total + 1;
    if (V < 1000)  return Total + 2;
    if (V < 10000) return Total + 3;
    V /= 10000;
    Total += 4;
  }
#endif
}

static inline const char* digitsGroup(Index Value) {
  return 
    &"0001020304050607080910111213141516171819"
     "2021222324252627282930313233343536373839"
     "4041424344454647484950515253545556575859"
     "6061626364656667686970717273747576777879"
     "8081828384858687888990919293949596979899"[Value * 2];
}

static inline void writeDigitsGroup(CharType** Out, Index* V) {
  const char* const Str = digitsGroup(*V % 100);
  *Out -= 2;
	CharType* writeStr = *Out;
  writeStr[0] = Str[0];
  writeStr[1] = Str[1];
  *V /= 100;
}

static inline errorCode integerToStringImpl(
	Index number, CharType* outStr, const Index lengthToLast, boolean isNegative)
{
	CharType *outPtr = NULL;

	CharType *Out = outPtr + lengthToLast;
	// Loop in groups of 100.
	while (number >= 100)
		writeDigitsGroup(&Out, &number);
	// If only one digit remains, write that and return.
  if (number < 10) {
		*(--Out) = (CharType)('0' + number);
	} else {
		writeDigitsGroup(&Out, &number);
	}

	if (isNegative)
		*(--Out) = '-';
	
	if EXIP_UNLIKELY(outPtr != Out)
		return EXIP_UNEXPECTED_ERROR;
	
	return EXIP_OK;
}

//////////////////////////////////////////////////////////////////////////

errorCode writeIntToBuffer(Integer number, CharType* dst, Index bufLen) {
	boolean isNegative = FALSE;

	if EXIP_UNLIKELY(dst == NULL)
		return EXIP_NULL_POINTER_REF;
	
	if (number < 0) {
		number = -number;
		isNegative = TRUE;
	}
	
	const Index length = countBase10(number) + isNegative;
	if (bufLen < length)
		return EXIP_OUT_OF_BOUND_BUFFER;

	dst[length] = '\0';
	return integerToStringImpl(
		(Index)number, dst, (length - 1), isNegative);
}

errorCode integerToString(Integer number, String* outStr)
{
	errorCode tmp_err_code;
	CharType *outPtr = NULL;
	boolean isNegative = FALSE;

	if (outStr == NULL)
		return EXIP_NULL_POINTER_REF;
	
	if (number < 0) {
		number = -number;
		isNegative = TRUE;
	}
	
	const Index length = countBase10(number) + isNegative;
	TRY(allocateStringMemory(&outPtr, length + 1));

	outPtr[length] = '\0';
	tmp_err_code = integerToStringImpl(
		(Index)number, outPtr, (length - 1), isNegative);
	
	if EXIP_UNLIKELY(tmp_err_code != EXIP_OK) {
		EXIP_MFREE(outPtr);
		return tmp_err_code;
	}

	outStr->str = outPtr;
	outStr->length = length;

	return EXIP_OK;
}

errorCode booleanToString(boolean b, String* outStr)
{
	errorCode tmp_err_code;
	CharType *outPtr;

	if (outStr == NULL)
		return EXIP_NULL_POINTER_REF;
	
	const Index length = 4 + (!b); 
	TRY(allocateStringMemory(&outPtr, length + 1));
	memcpy(outPtr, b ? "TRUE" : "FALSE", length + 1);

	outStr->str = outPtr;
	outStr->length = length;

	return EXIP_OK;
}

errorCode floatToString(Float f, String* outStr)
{
	(void)f;
	return EXIP_NOT_IMPLEMENTED_YET;
}

errorCode decimalToString(Decimal d, String* outStr)
{
	(void)d;
	return EXIP_NOT_IMPLEMENTED_YET;
}

errorCode dateTimeToString(EXIPDateTime dt, String* outStr)
{
	(void)dt;
	return EXIP_NOT_IMPLEMENTED_YET;
}

#endif /* EXIP_IMPLICIT_DATA_TYPE_CONVERSION */

#if EXIP_DEBUG == ON

void printString(const String* inStr)
{
	if(inStr->length == 0)
		return;
#if EXIP_DEBUG_LEVEL < WARNING
	if(debugMode == ON)
		DEBUG_OUTPUT(("%.*s", (int) inStr->length, inStr->str));
#endif
}

#endif /* EXIP_DEBUG */
