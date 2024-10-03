/*==================================================================*\
|                EXIP - Embeddable EXI Processor in C                |
|--------------------------------------------------------------------|
|          This work is licensed under BSD 3-Clause License          |
|  The full license terms and conditions are located in LICENSE.txt  |
\===================================================================*/

/**
 * @file errorHandle.c
 * @brief Implementation of debug printing.
 *
 * @date Oct 3, 2024
 * @author Eightfold
 * @version 0.5
 * @par[Revision] $Id$
 */

#include "errorHandle.h"
#include "procTypes.h"

#define COPY_LITERAL(pdst, src) copyStringData(pdst, src, (sizeof(src) - 1))
#define FUNC_TEXT_HEAD "'"
#define FUNC_TEXT_TAIL "' "

#define RESET   "\033[0m"
#define RED     "\033[31;1m"
#define GREEN   "\033[32;1m"
#define BLUE    "\033[34;1m"
#define YELLO   "\033[33;1m"
#define CYAN    "\033[36;1m"
#define WHITE   "\033[37;1m"

enum ErrorExtra_ {
  FUNC_NAME_SIZE  = 50,
  FUNC_TEXT_SIZE = sizeof(FUNC_TEXT_HEAD FUNC_TEXT_TAIL) - 1,
  FUNC_XTRA_SIZE = sizeof("...") - 1,
  FUNC_NAME_MAX   = FUNC_NAME_SIZE + FUNC_TEXT_SIZE + FUNC_XTRA_SIZE,
};

int ansiMode = 1;

static void copyStringData(char** pdst, const char* src, size_t len);
static const char* getFileName(const char* name);
static errorCode formatFuncData(char funcData[], const char* function);
static const char* getNextline(const char** ptext);

#if EXIP_ANSI
static void debugPrintAnsi(
  errorCode err, const char* text,
  const char* filename, const char* funcData, int line)
{
  const char* nextline = getNextline(&text);
  DEBUG_OUTPUT((
    RED   "\n>Error %s"
    RESET " in "
    BLUE  "%s"
    CYAN  "[\"%s\":%d]"
    WHITE "%s%s" RESET,
    // Inputs
    GET_ERR_STRING(err),
    funcData,
    filename, line,
    nextline, text
  ));
}
#endif // EXIP_ANSI

static void debugPrintNorm(
  errorCode err, const char* text,
  const char* filename, const char* funcData, int line)
{
  const char* nextline = getNextline(&text);
  DEBUG_OUTPUT((
    "\n>Error %s in %s"
    "[\"%s\":%d]"
    "%s%s",
    GET_ERR_STRING(err), funcData,
    filename, line,
    nextline, text
  ));
}

void exipDebugPrint(
  errorCode err, const char* text,
  const char* filename, const char* function, int line)
{
  const char* shortFilename = getFileName(filename);
  char funcData[FUNC_NAME_MAX + 1];
  formatFuncData(funcData, function);
  const char* outFuncData = (function && *function) ? funcData : NULL;

#if EXIP_ANSI
  if (!!ansiMode) {
    debugPrintAnsi(err, text, shortFilename, outFuncData, line);
    return;
  }
#endif
  debugPrintNorm(err, text, shortFilename, funcData, line);
}

//////////////////////////////////////////////////////////////////////////

static const char* getFileName(const char* name) {
	if (!name)
		return "";
#if EXIP_DEBUG
	const size_t len = strlen(name);
  size_t hitCount = 0;
	const char* end = (name + len);
	while ((--end) != name) {
		const char c = *end;
		if (c == '/' || c == '\\')
      ++hitCount;
		if (hitCount == 3)
      return end + 1;
	}
#endif
	return name;
}

static errorCode formatFuncData(char funcData[], const char* function)
{
  if (!funcData) {
    return EXIP_NULL_POINTER_REF;
  } else if (!function) {
    funcData[0] = '\0';
    return EXIP_OK;
  }

  COPY_LITERAL(&funcData, FUNC_TEXT_HEAD);

  const size_t len = strlen(function);
  if (len <= FUNC_NAME_SIZE) {
    copyStringData(&funcData, function, len);
  } else {
    copyStringData(&funcData, function, FUNC_NAME_SIZE);
    COPY_LITERAL(&funcData, "...");
  }

  COPY_LITERAL(&funcData, FUNC_TEXT_TAIL);
  *funcData = '\0';
  return EXIP_OK;
}


void copyStringData(char** pdst, const char* src, size_t len) {
  if (!src)
    return;
  if (len == 0)
    len = strlen(src);
  memcpy(*pdst, src, len);
  *pdst = (*pdst) + len;
}

const char* getNextline(const char** ptext) {
  if (!*ptext) {
    *ptext = "";
    return "";
  }
  return "\n > ";
}
