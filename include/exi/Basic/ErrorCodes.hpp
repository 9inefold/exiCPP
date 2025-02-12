//===- exi/Basic/ErrorCodes.hpp -------------------------------------===//
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

#pragma once

#include "core/Common/Fundamental.hpp"
#include "core/Common/StrRef.hpp"

namespace exi {

class raw_ostream;
template <typename> class SmallVecImpl;

enum class ErrorCode : i32 {
  kOk      = 0,
  kSuccess = 0,

  /// Tell the exicpp parser to stop parsing.
	kStop,

  /// End of the buffer has been reached.
  kBufferEndReached,

	/// Parsing has been completed.
	kParsingComplete,

  /// The code for this function is not yet implemented.
	kUnimplemented,

	/// Any error that does not fall into the other categories.
	kUnexpectedError,

	/// Array access out of bounds.
	kOutOfBounds,
	kNullptrRef,

	//// Unsuccessful memory allocation.
	kInvalidMemoryAlloc,
	kInvalidEXIHeader,

	/// Processor state is inconsistent with the stream events.
	kInconsistentProcState,
	kInvalidEXIInput,

  /// The information passed to the EXIP API is invalid.
	kInvalidConfig,

  /// When encoding XML Schema in EXI the prefixes must be preserved:
  /// When qualified `namesNS` are used in the values of AT or CH events in an
  /// EXI Stream, the `Preserve.prefixes` fidelity option SHOULD be turned on to
  /// enable the preservation of the NS prefix declarations used by these values.
  /// Note, in particular among other cases, that this practice applies to the
  /// use of `xsi:type` attributes in EXI streams when `Preserve.lexicalValues`
  /// fidelity option is set to true.
  kNoPrefixesPreservedXMLSchema,
	kInvalidStringOp,

	/// Mismatch in the header options. This error can be due to:
  ///
	/// 1) The "alignment" element MUST NOT appear in an EXI options document
  ///    when the "compression" element is present;
	/// 2) The "strict" element MUST NOT appear in an EXI options document when
  ///    one of "dtd", "prefixes", "comments", "pis" or "selfContained" element
  ///    is present in the same options document. That is only the element "lexicalValues", 
  ///    from the fidelity options, is permitted to occur in the presence of "strict" element;
	/// 3) The "selfContained" element MUST NOT appear in an EXI options document
  ///    when one of "compression", "pre-compression" or "strict" elements are
  ///    present in the same options document.
	/// 4) The `DatatypeRepresentationMap` option does not take effect when the
  ///    value of the `Preserve.lexicalValues` fidelity option is true (see 6.3 Fidelity Options),
  ///    or when the EXI stream is a schema-less EXI stream.
	/// 5) Presence Bit for EXI Options not set and no out-of-band options set.
	kHeaderOptionsMismatch,
	kERROR_LAST,
};

inline constexpr usize kErrorCodeMax = i32(ErrorCode::kERROR_LAST);
StrRef get_error_name(ErrorCode E) noexcept EXI_READONLY;
StrRef get_error_message(ErrorCode E) noexcept EXI_READONLY;

/// Works like `Error`, returns `true` when a non-ok state is held.
class EXI_NODISCARD ExiError {
  ErrorCode EC;
  u32 Extra = 0;

  constexpr ExiError(ErrorCode E, u32 Extra) : EC(E), Extra(Extra) {}
public:
  using enum ErrorCode;
  static constexpr u32 Unset = u32(-1);

  constexpr ExiError(ErrorCode E) : EC(E) {}
  static ExiError New(ErrorCode E) noexcept EXI_READONLY;

  constexpr static ExiError Full() {
    return ExiError(kBufferEndReached, Unset);
  }
  static ExiError Full(i64 Bits) noexcept EXI_READONLY;

  static const ExiError OK;
  static const ExiError STOP;
  static const ExiError DONE;
  static const ExiError FULL;
  static const ExiError TODO;
  static const ExiError OOB;

  ////////////////////////////////////////////////////////////////////////
  // Observers

  ErrorCode ec() const { return EC; }
  const char* what() const noexcept EXI_READONLY;
  StrRef msg() const noexcept EXI_READONLY;
  /// Gets message with any custom information.
  StrRef msg(SmallVecImpl<char>& Vec) const;

  bool isSpecialCase() const;
  void print(raw_ostream& OS) const;
  void toVector(SmallVecImpl<char>& Vec) const;

  explicit operator ErrorCode() const { return EC; }
  explicit operator bool() const {
    return EXI_LIKELY(EC != ErrorCode::kSuccess);
  }

  friend bool operator==(const ExiError& LHS, const ExiError& RHS) {
    return LHS.EC == RHS.EC;
  }
};

inline bool operator==(const ExiError& LHS, ErrorCode RHS) {
  return LHS.ec() == RHS;
}

inline bool operator==(ErrorCode LHS, const ExiError& RHS) {
  return LHS == RHS.ec();
}

raw_ostream& operator<<(raw_ostream& OS, ErrorCode Err);
raw_ostream& operator<<(raw_ostream& OS, const ExiError& Err);

EXI_CONST ExiError ExiError::OK   = ErrorCode::kOk;
EXI_CONST ExiError ExiError::STOP = ErrorCode::kStop;
EXI_CONST ExiError ExiError::DONE = ErrorCode::kParsingComplete;
EXI_CONST ExiError ExiError::FULL = ExiError::Full();
EXI_CONST ExiError ExiError::TODO = ErrorCode::kUnimplemented;
EXI_CONST ExiError ExiError::OOB  = ErrorCode::kOutOfBounds;

} // namespace exi
