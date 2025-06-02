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

#include <core/Common/Fundamental.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/Result.hpp>
#include <system_error>

namespace exi {

class raw_ostream;
template <typename> class SmallVecImpl;

enum class AlignKind : u8;
class PreserveBuilder;
enum class EventTerm : i32;

enum class ErrorCode : u32 {
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

  /// The information passed to the EXI API is invalid.
  // TODO: Find uses of this in EXIP?
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
  Last = kHeaderOptionsMismatch
};

/// Suboptions for `InvalidEXIHeader` and `HeaderOptionsMismatch`.
enum class InvalidHeaderCode : u8 {
  kDefault = 0,         // Default, no specific code.
  kCookie,              // Cookie is not `$EXI`
  kDistinguishingBits,  // Bits are not `0b10`
  kInvalidVersion,      // Version is not `Final Version 1`
  
  kMixedAlignment,      // Mixing alignment and compression
  kStrictPreserved,     // `preserve.X` mixed with `strict`
  kSelfContained,       // `selfContained` incompatible with options
  kDatatypeMap,         // ...
  kOutOfBandOpts,       // No options, but out-of-band not specified
  Last = kOutOfBandOpts
};

StrRef get_error_name(ErrorCode E) EXI_READNONE;
StrRef get_error_message(ErrorCode E) EXI_READNONE;

/// Works like `Error`, returns `true` when a non-ok state is held.
class EXI_NODISCARD alignas(8) ExiError {
  ErrorCode EC;
  u32 Extra = 0;
  union {
    u64 Storage;
    const std::error_category* Category;
  };
  
  constexpr ExiError(ErrorCode E, u32 Extra) :
   EC(E), Extra(Extra), Storage(0) {}
  
public:
  using enum ErrorCode;
  static constexpr u32 Inval = u32(-1);
  static constexpr u32 Unset = u32(-1);

  static const ExiError OK;
  static const ExiError STOP;
  static const ExiError DONE;
  static const ExiError FULL;
  static const ExiError TODO;
  static const ExiError OOB;

  ////////////////////////////////////////////////////////////////////////
  // Ctors
  
  /// Construct an error from a code.
  constexpr ExiError(ErrorCode E) : EC(E) {}
  /// Construct an error from a code.
  static ExiError New(ErrorCode E) EXI_READONLY;

  // template <typename T>
  // constexpr ExiError(Unexpect<T> Val) : ExiError(Val.error()) {}
  constexpr ExiError(Unexpect<ExiError> Val) : ExiError(Val.error()) {}

  /// Default full error code.
  constexpr static ExiError Full() {
    return ExiError(kBufferEndReached, Unset);
  }
  /// Create a full error code with the amount of bits read.
  static ExiError Full(i64 Bits) EXI_READONLY;

  /// Default invalid header error code.
  constexpr static ExiError Header() {
    return ExiError(kInvalidEXIHeader);
  }
  /// Default mismatch header error code.
  // FIXME: Add specific error messages.
  constexpr static ExiError Mismatch() {
    return ExiError(kHeaderOptionsMismatch);
  }

  /// Invalid character in EXI signature.
  static ExiError HeaderSig(char C) EXI_READONLY;
  /// Invalid distinguishing bits sequence.
  static ExiError HeaderBits(u64 Bits) EXI_READONLY;
  /// Invalid EXI Version - Preview.
  static ExiError HeaderVer() EXI_READONLY;
  /// Invalid EXI Version - Final.
  static ExiError HeaderVer(u64 Version) EXI_READONLY;
  /// Mismatched alignment settings.
  static ExiError HeaderAlign(
   AlignKind A, bool Compress = false) EXI_READONLY;
  /// Mismatched Preserve settings.
  static ExiError HeaderStrict(PreserveBuilder Opt) EXI_READONLY;
  /// Mismatched Preserve settings.
  static ExiError HeaderSelfContained(
   AlignKind A, bool Strict = false) EXI_READONLY;
  /// Out of band without provided options.
  static ExiError HeaderOutOfBand() EXI_READONLY;

  ////////////////////////////////////////////////////////////////////////
  // Observers

  ErrorCode ec() const { return EC; }
  const char* what() const EXI_READONLY;
  StrRef msg() const EXI_READONLY;
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

EXI_CONST ExiError ExiError::OK   = ExiError::kOk;
EXI_CONST ExiError ExiError::STOP = ExiError::kStop;
EXI_CONST ExiError ExiError::DONE = ExiError::kParsingComplete;
EXI_CONST ExiError ExiError::FULL = ExiError::Full();
EXI_CONST ExiError ExiError::TODO = ExiError::kUnimplemented;
EXI_CONST ExiError ExiError::OOB  = ExiError::kOutOfBounds;

static_assert(sizeof(ExiError) == 16);

template <typename T>
using ExiResult = Result<T, ExiError>;

} // namespace exi
