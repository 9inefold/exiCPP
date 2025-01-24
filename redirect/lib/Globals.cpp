//===- Globals.cpp --------------------------------------------------===//
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
///
/// \file
/// This file defines globals used by exi::redirect.
///
//===----------------------------------------------------------------===//

#include <Globals.hpp>
#include <Fundamental.hpp>
#if EXI_DEBUG
# include <ArrayRef.hpp>
# include <Strings.hpp>
# include "NtImports.hpp"
#endif

using namespace re;

#if EXI_DEBUG
extern "C" {
NTSYSAPI ULONG DbgPrint(PCSTR Fmt, ...);
} // extern "C"

static bool AssertInPrint = false;

static void PrintSimpleExpr(const char* Expr) {
  (void) DbgPrint("Assertion `%s` failed.\n", Expr);
}

static void PrintMsg(const char* Msg) {
  (void) DbgPrint("Assertion failed: %s.\n", Msg);
}

static void PrintDbg(const char* Msg, const char* Expr) {
  if (!Msg || *Msg == '\0')
    return PrintSimpleExpr(Expr);

  ArrayRef MsgStr(Msg, Strlen(Msg));
  ArrayRef ExprStr(Expr, Strlen(Expr));

  if (!ExprStr.ends_with('"'))
    return PrintMsg(Msg);
  
  ExprStr = ExprStr.drop_back();
  if (!ExprStr.ends_with(MsgStr))
    return PrintMsg(Msg);
  ExprStr = ExprStr.drop_back();

  for (char Back = ExprStr.back(); Back != ',';) {
    if (ExprStr.empty())
      return PrintMsg(Msg);
    Back = ExprStr.back();
    ExprStr = ExprStr.drop_back();
  }

  int ExprSize = ExprStr.size();
  (void) DbgPrint("Assertion `%*s` failed: %s.\n",
    int(ExprSize), Expr, Msg);
} 

/// Use with breakpoints.
RE_COLD void re::re_assert_failed(const char* Msg, const char* Expr) {
  if (not AssertInPrint && Expr) {
    AssertInPrint = true;
    PrintDbg(Msg, Expr);
    AssertInPrint = false;
  }
  RE_TRAP;
}
#endif
