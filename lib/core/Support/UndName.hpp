//===- Support/RTTI.cpp ---------------------------------------------===//
//===- Support/UndName.cpp ------------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file wraps the api for demangling MSVC internal typenames.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Fundamental.hpp>
#include <cstdlib>
#include <climits>
#include <memory>

// Reference: https://github.com/nihilus/IDA_ClassInformer

namespace exi {

extern "C" {
using UndAlloc = void*(__cdecl*)(size_t);
using UndFree  = void(__cdecl*)(void*);
} // extern "C"

/// Flags for the method of undecoration.
struct UndStrategy {
  enum type : u32 {
    Complete              = 0x00000,  // Enable full undecoration
    NoLeadingUnderscores  = 0x00001,  // Remove leading underscores from MS extended keywords
    NoMsKeywords          = 0x00002,  // Disable expansion of MS extended keywords
    NoFunctionReturns     = 0x00004,  // Disable expansion of return type for primary declaration
    NoAllocationModel     = 0x00008,  // Disable expansion of the declaration model
    NoAllocationLanguage  = 0x00010,  // Disable expansion of the declaration language specifier
    NoMSThisType          = 0x00020,  // Disable expansion of MS keywords on the 'this' type for primary declaration
    NoCVThisType          = 0x00040,  // Disable expansion of CV modifiers on the 'this' type for primary declaration
    NoThisType            = 0x00060,  // Disable all modifiers on the 'this' type
    NoAccessSpecifiers    = 0x00080,  // Disable expansion of access specifiers for members
    NoThrowSignatures     = 0x00100,  // Disable expansion of 'throw-signatures' for functions and pointers to functions
    NoMemberType          = 0x00200,  // Disable expansion of 'static' or 'virtual'ness of members
    NoReturnUDTModel      = 0x00400,  // Disable expansion of MS model for UDT returns
    Decode32Bit           = 0x00800,  // Undecorate 32-bit decorated names
    NameOnly              = 0x01000,  // Crack only the name for primary declaration; return just [scope::]name.  Does expand template params
    TypeOnly              = 0x02000,  // Input is just a type encoding; compose an abstract declarator
    HaveParameters        = 0x04000,  // The real templates parameters are available
    NoECSU                = 0x08000,  // Suppress enum/class/struct/union
    NoIdentCharCheck      = 0x10000,  // Suppress check for IsValidIdentChar
  };

  using Type = type;
};

extern "C" char* __cdecl __unDName( // NOLINT
  char* Buffer, const char* Symbol, int Size,
  UndAlloc Alloc, UndFree Free, 
  UndStrategy::Type Flags
);

} // namespace exi
