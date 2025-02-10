//===- exi/Basic/ErrorCodes.cpp -------------------------------------===//
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
/// This file defines the error codes used by the program.
///
//===----------------------------------------------------------------===//

#include "exi/Basic/ErrorCodes.hpp"

using namespace exi;

static constexpr const char* ErrorCodeMessages[kErrorCodeMax + 1] {
  "Success",
	"Stop",
	"Unimplemented",
	"UnexpectedError",
	"OutOfBounds",
	"NullptrRef",
	"InvalidMemoryAlloc",
	"InvalidEXIHeader",
	"InconsistentProcState",
	"InvalidEXIInput",
	"BufferEndReached",
	"ParsingComplete",
	"InvalidConfig",
  "NoPrefixesPreservedXMLSchema",
	"InvalidStringOp",
	"HeaderOptionsMismatch"
};

inline static const char*
 get_error_message_what(ErrorCode E) noexcept {
  const usize Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kErrorCodeMax)
    return ErrorCodeMessages[Ix];
  return "UNKNOWN ERROR";
}

StrRef exi::get_error_message(ErrorCode E) noexcept {
  return StrRef(get_error_message_what(E));
}

//////////////////////////////////////////////////////////////////////////
// Error Wrapper

ExiError ExiError::New(ErrorCode E) noexcept { return ExiError(E); }

const char* ExiError::what() const noexcept {
  return get_error_message_what(this->EC);
}

StrRef ExiError::msg() const noexcept {
  return exi::get_error_message(this->EC);
}
