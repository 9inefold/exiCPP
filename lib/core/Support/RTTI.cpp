//===- Support/RTTI.cpp ---------------------------------------------===//
//
// Copyright (C) 2024-2025 Ninefold
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
/// This file provides an API for getting demangled type names.
///
//===----------------------------------------------------------------===//

#include <Support/RTTI.hpp>
//#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Config/Config.inc>
#include <Demangle/Demangle.hpp>
#include <Demangle/StringViewExtras.hpp>
#include <Support/Allocator.hpp>
#include <Support/raw_ostream.hpp>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

using namespace exi;
using namespace exi::rtti;

/// Calls the implementation-specific demangling API.
static RttiResult<String> DemangleSymbol(StrRef MangledName);

static StrRef AppendToBuffer(StrRef S, SmallVecImpl<char>& Buf) {
  Buf.clear();
  Buf.reserve(S.size());
  Buf.append(S.begin(), S.end());
  return StrRef(Buf.begin(), S.size());
}

template <typename CB>
ALWAYS_INLINE static auto RttiDemangleCommon(StrRef Symbol, CB&& Callable)
 -> RttiResult<std::invoke_result_t<CB, StrRef>> {
  try {
    auto Out = DemangleSymbol(Symbol);
    if (Out.is_err())
      return Err(Out.error());
    return EXI_FWD(Callable)(*Out);
  } catch (const std::bad_alloc&) {
    return Err(RttiError::InvalidMemoryAlloc);
  }
}

static RttiResult<String> RttiDemangleImpl(StrRef Symbol) {
  try {
    return DemangleSymbol(Symbol);
  } catch (const std::bad_alloc&) {
    return Err(RttiError::InvalidMemoryAlloc);
  }
}

static RttiResult<StrRef> RttiDemangleImpl(StrRef Symbol, SmallVecImpl<char>& Buf) {
  try {
    RttiResult<String> OutOrErr = DemangleSymbol(Symbol);
    if (OutOrErr.is_err())
      return Err(OutOrErr.error());
    return AppendToBuffer(*OutOrErr, Buf);
  } catch (const std::bad_alloc&) {
    return Err(RttiError::InvalidMemoryAlloc);
  }
}

static RttiError RttiDemangleImpl(StrRef Symbol, raw_ostream& OS) {
  try {
    RttiResult<String> Out = DemangleSymbol(Symbol);
    if (Out.is_err())
      return Out.error();
    OS << *Out;
    return RttiError::Success;
  } catch (const std::bad_alloc&) {
    return RttiError::InvalidMemoryAlloc;
  }
}

//===----------------------------------------------------------------===//
// Exposed API
//===----------------------------------------------------------------===//

static Option<RttiError> DemangleChk(const char* Symbol) {
  if EXI_UNLIKELY(!Symbol)
    return RttiError::InvalidArgument;
  else if EXI_UNLIKELY(Symbol[0] == '\0')
    return RttiError::InvalidName;
  else
    return std::nullopt;
}

static Option<RttiError> DemangleChk(StrRef Symbol) {
  if EXI_UNLIKELY(Symbol.empty())
    return RttiError::InvalidName;
  else
    return std::nullopt;
}

//////////////////////////////////////////////////////////////////////////
// Empty

RttiResult<String> exi::rtti::demangle(const char* Symbol) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  else
    return RttiDemangleImpl(StrRef(Symbol));
}

RttiResult<String> exi::rtti::demangle(StrRef Symbol) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  return RttiDemangleImpl(Symbol);
}

#if !EXI_COMPILER(MSVC)
RttiResult<String> exi::rtti::demangle(const std::type_info& Info) {
  return RttiDemangleImpl(StrRef(Info.name()));
}
#endif // !MSVC

//////////////////////////////////////////////////////////////////////////
// Buffered

RttiResult<StrRef> exi::rtti::demangle(
 const char* Symbol, SmallVecImpl<char>& Buf) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  else
    return RttiDemangleImpl(StrRef(Symbol), Buf);
}

RttiResult<StrRef> exi::rtti::demangle(
 StrRef Symbol, SmallVecImpl<char>& Buf) {
  if (auto OptErr = DemangleChk(Symbol))
    return Err(*OptErr);
  return RttiDemangleImpl(Symbol, Buf);
}

RttiResult<StrRef> exi::rtti::demangle(
 const std::type_info& Info, SmallVecImpl<char>& Buf) {
  return RttiDemangleImpl(StrRef(Info.name()), Buf);
}

//////////////////////////////////////////////////////////////////////////
// raw_ostream

RttiError exi::rtti::demangle(const char* Symbol, raw_ostream& OS) {
  if (auto OptErr = DemangleChk(Symbol))
    return *OptErr;
  else
    return RttiDemangleImpl(StrRef(Symbol), OS);
}

RttiError exi::rtti::demangle(StrRef Symbol, raw_ostream& OS) {
  if (auto OptErr = DemangleChk(Symbol))
    return *OptErr;
  return RttiDemangleImpl(Symbol, OS);
}

RttiError exi::rtti::demangle(
 const std::type_info& Info, raw_ostream& OS) {
  return RttiDemangleImpl(StrRef(Info.name()), OS);
}

//===----------------------------------------------------------------===//
// Implementation
//===----------------------------------------------------------------===//

/*
using Demangler = itanium_demangle::ManglingParser<DefaultAllocator>;

namespace {
enum : int {
  demangle_invalid_args = -3,
  demangle_invalid_mangled_name = -2,
  demangle_memory_alloc_failure = -1,
  demangle_success = 0,
};
}

extern "C" char * __cxa_demangle(const char *MangledName, char *Buf, size_t *N, int *Status) {
  if (MangledName == nullptr || (Buf != nullptr && N == nullptr)) {
    if (Status)
      *Status = demangle_invalid_args;
    return nullptr;
  }

  int InternalStatus = demangle_success;
  Demangler Parser(MangledName, MangledName + std::strlen(MangledName));
  Node *AST = Parser.parse();

  if (AST == nullptr)
    InternalStatus = demangle_invalid_mangled_name;
  else {
    OutputBuffer O(Buf, N);
    DEMANGLE_ASSERT(Parser.ForwardTemplateRefs.empty(), "");
    AST->print(O);
    O += '\0';
    if (N != nullptr)
      *N = O.getCurrentPosition();
    Buf = O.getBuffer();
  }

  if (Status)
    *Status = InternalStatus;
  return InternalStatus == demangle_success ? Buf : nullptr;
}
*/

// TODO: Add option to remove stuff like ABI tags.
static RttiResult<String> DemangleSymbol(StrRef MangledName) {
  std::string Result;

  if (exi::nonMicrosoftDemangle(MangledName, Result))
    return Result;

  if (MangledName.starts_with('_')) {
    if (exi::nonMicrosoftDemangle(MangledName.substr(1), Result,
                                 /*CanHaveLeadingDot=*/false)) {
      return Result;
    }
  }

  auto Flags = MSDF_NoAccessSpecifier
             | MSDF_NoCallingConvention;
  char *Demangled = exi::microsoftDemangle(MangledName, nullptr, nullptr, MSDemangleFlags(Flags));
  if (Demangled == nullptr)
    return Err(RttiError::InvalidName);
  
  Result = Demangled;
  exi::exi_free(Demangled);
  return Result;
}
