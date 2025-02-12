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
#include "core/Common/SmallStr.hpp"
#include "core/Support/IntCast.hpp"
#include "core/Support/raw_ostream.hpp"

using namespace exi;

static constexpr const char* ErrorCodeNames[kErrorCodeMax + 1] {
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

static constexpr const char* ErrorCodeMessages[kErrorCodeMax + 1] {
  "Success",
	"Stop",
	"Unimplemented",
	"Unexpected",
	"Out Of Bounds",
	"Nullptr Reference",
	"Invalid Memory Allocation",
	"Invalid EXI Header",
	"Inconsistent Processor State",
	"Invalid EXI Input",
	"Buffer End Reached",
	"Parsing Complete",
	"Invalid Configuration",
  "No Prefixes Preserved XML Schema",
	"Invalid String Operation",
	"Header Options Mismatch"
};

inline static const char*
 get_error_name_what(ErrorCode E) noexcept {
  const usize Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kErrorCodeMax)
    return ErrorCodeNames[Ix];
  return "UNKNOWN_ERROR";
}

inline static const char*
 get_error_message_what(ErrorCode E) noexcept {
  const usize Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kErrorCodeMax)
    return ErrorCodeMessages[Ix];
  return "UNKNOWN ERROR";
}

StrRef exi::get_error_name(ErrorCode E) noexcept {
  return StrRef(get_error_name_what(E));
}
StrRef exi::get_error_message(ErrorCode E) noexcept {
  return StrRef(get_error_message_what(E));
}

raw_ostream& exi::operator<<(raw_ostream& OS, ErrorCode Err) {
	return OS << exi::get_error_message(Err);
}

//////////////////////////////////////////////////////////////////////////
// Error

ExiError ExiError::New(ErrorCode E) noexcept {
	return ExiError(E);
}

ExiError ExiError::Full(i64 Bits) noexcept {
	if EXI_UNLIKELY(!CheckIntCast<u32>(Bits))
		return ExiError::Full();
	return ExiError(kBufferEndReached, static_cast<u32>(Bits));
}

const char* ExiError::what() const noexcept {
  return get_error_message_what(this->EC);
}

StrRef ExiError::msg() const noexcept {
  return exi::get_error_message(this->EC);
}

raw_ostream& exi::operator<<(raw_ostream& OS, const ExiError& Err) {
	Err.print(OS);
	return OS;
}

//////////////////////////////////////////////////////////////////////////
// Special Handling

bool ExiError::isSpecialCase() const {
	switch (this->EC) {
	case kBufferEndReached:
		return Extra != ExiError::Unset;
	case kUnimplemented: // TODO
	default:
		return false;
	}
}

static void FormatBER(raw_ostream& OS, u32 Bits) {
	const auto NBytes = IntCastOrZero<i64>(Bits);
	if (NBytes == 0) {
		OS << "Buffer completely full";
		return;
	}

	OS << "Buffer full, unable to read ";
	if (NBytes % kCHAR_BIT == 0) {
		OS << (NBytes / kCHAR_BIT) << "-byte integer";
	} else {
		OS << NBytes << "-bit integer";
	}
}

void ExiError::print(raw_ostream& OS) const {
	OS << "EXI Error: ";
	switch (this->EC) {
	case kBufferEndReached:
		return FormatBER(OS, this->Extra);
	default:
		OS << get_error_message_what(EC);
	}
}

void ExiError::toVector(SmallVecImpl<char>& Vec) const {
	raw_svector_ostream OS(Vec);
  this->print(OS);
}

StrRef ExiError::msg(SmallVecImpl<char>& Vec) const {
	if (not this->isSpecialCase())
		return exi::get_error_message(EC);
	toVector(Vec);
	return StrRef(Vec.begin(), Vec.size());
}
