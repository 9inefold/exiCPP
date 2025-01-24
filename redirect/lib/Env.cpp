//===- Env.cpp ------------------------------------------------------===//
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

#include <Env.hpp>
#include <Buf.hpp>
#include <Logging.hpp>
#include <Strings.hpp>
#include "NtImports.hpp"

using namespace re;

static UnicodeString LoadEnvStr(const char* Env, WNameBuf& UBuf) {
  AnsiString EnvStr;
  RtlInitAnsiString(&EnvStr, Env);

  UnicodeString EnvUStr;
  UBuf.setNt(EnvUStr);

  RtlAnsiStringToUnicodeString(&EnvUStr, &EnvStr, false);
  return EnvUStr;
}

static void CloneUStrToStr(WNameBuf& UBuf, NameBuf& Buf) {
  UnicodeString UStr;
  UBuf.setNt(UStr);
  Buf.loadNt_U(UStr);
}

static bool DoQuery(UnicodeString UEnv, WNameBuf& UNameBuf) {
  i32 Status = RtlQueryEnvironmentVariable(
    nullptr,
    UEnv.data(), UEnv.size(),
    UNameBuf.data(), UNameBuf.capacity(),
    &UNameBuf.Size
  );

  if (Status < 0) {
    if (Status != 0xC0000100)
      MiTrace("ENV ERROR: %#x", u32(Status));
    return false;  
  }

  return true;
}

bool re::FindEnvironmentVariable(const char* Env, NameBuf& Buf) {
  Buf.Size = 0;
  *Buf.Data = '\0';
  if (!Env)
    return false;

  WNameBuf EnvBuf {};
  UnicodeString UEnv = LoadEnvStr(Env, EnvBuf);

  WNameBuf UNameBuf {};
  if (!DoQuery(UEnv, UNameBuf))
    return false;

  if (UNameBuf.size() > Buf.capacity()) {
    return false;
  }

  CloneUStrToStr(UNameBuf, Buf);
  return true;
}

//////////////////////////////////////////////////////////////////////////


bool re::FindEnvInParamList(const NameBuf& Buf, bool ParamListType) {
  const char* const Data = Buf.data();
  const usize Size = Buf.size();
  const char* List = ParamListType ? kEnvTrue : kEnvFalse;

  while (true) {
    // Advance to first match.
    List = StrchrInsensitive(List, *Data);
    if (!List)
      return false;
    
    if (StrequalInsensitive(List, Data, Size))
      // Found a match.
      break;
    // Advance past match.
    ++List;
  }

  return true;
}
