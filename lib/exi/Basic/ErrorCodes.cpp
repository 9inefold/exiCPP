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
#include "core/Common/StringExtras.hpp"
#include "core/Common/bit.hpp"
#include "core/Support/Debug.hpp"
#include "core/Support/Format.hpp"
#include "core/Support/IntCast.hpp"
#include "core/Support/Limits.hpp"
#include "core/Support/Logging.hpp"
#include "core/Support/raw_ostream.hpp"
#include "exi/Basic/ProcTypes.hpp"

#define DEBUG_TYPE "ErrorCodes"

using namespace exi;

using IHCType = exi::InvalidHeaderCode;

static constexpr const char* ErrorCodeNames[kErrorCodeCount + 1] {
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

static constexpr const char* ErrorCodeMessages[kErrorCodeCount + 1] {
  "Success",
	"Stop",
	"Unimplemented Behaviour",
	"Unexpected Error",
	"Attempted Out Of Bounds Access",
	"Nullptr Reference",
	"Memory Allocation Failed :(",
	"Invalid EXI Header",
	"Inconsistent Processor State",
	"Invalid EXI Input",
	"Buffer End Reached",
	"Parsing Complete",
	"Invalid EXI Configuration",
  "No Prefixes Preserved XML Schema",
	"Invalid String Operation",
	"Mismatched Header Options"
};

inline static const char*
 get_error_name_what(ErrorCode E) noexcept {
  const usize Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kErrorCodeCount)
    return ErrorCodeNames[Ix];
  return "UNKNOWN_ERROR";
}

inline static const char*
 get_error_message_what(ErrorCode E) noexcept {
  const usize Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kErrorCodeCount)
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

const char* ExiError::what() const noexcept {
  return get_error_message_what(this->EC);
}

StrRef ExiError::msg() const noexcept {
  return exi::get_error_message(this->EC);
}

raw_ostream& exi::operator<<(raw_ostream& OS, const ExiError& Err) {
	SmallStr<80> Str;
	return OS << Err.msg(Str);
}

//======================================================================//
// Special Handling
//======================================================================//

//////////////////////////////////////////////////////////////////////////
// InvalidEXIHeader

namespace {

struct PackedAlign {
	u16 AlignV 		: 4;
	u16 Compress 	: 1;
	u16 Strict		: 1;
};

struct HeaderExtra {
	IHCType IHC;
	union {
		u16 	Default;
		char 	Cookie;
		u16 	Bits;
		u16 	Version;
		PreserveKind StrictOpts;
		PackedAlign Align;
		PackedAlign SelfContained;
	};
};

static_assert(sizeof(PackedAlign) == sizeof(u16));
static_assert(sizeof(HeaderExtra) == sizeof(u32));

static PackedAlign PackAlign(
 AlignKind A, bool Compress, bool Strict = false) {
	return {
		.AlignV 	= u16(A),
		.Compress = Compress,
		.Strict 	= Strict
	};
}

static u32 FromHeader(HeaderExtra Ex) {
#if EXI_HAS_BUILTIN(__builtin_clear_padding)
	__builtin_clear_padding(&Ex);
#endif
	return std::bit_cast<u32>(Ex);
}

static HeaderExtra ToHeader(u32 Ex) {
	return std::bit_cast<HeaderExtra>(Ex);
}

} // namespace `anonymous`

#define FROM_HEADER(EC, ...) ExiError(EC, FromHeader(HeaderExtra{__VA_ARGS__}))

ExiError ExiError::HeaderSig(char C) noexcept {
	return FROM_HEADER(kInvalidEXIHeader,
		.IHC = IHCType::kCookie,
		.Cookie = (exi::isPrint(C) ? C : '\0')
	);
}

ExiError ExiError::HeaderBits(u64 Bits) noexcept {
	const u16 DBits = (Bits <= 0b11)
		? u16(Bits) : exi::max_v<u16>;
	return FROM_HEADER(kInvalidEXIHeader,
		.IHC = IHCType::kDistinguishingBits,
		.Bits = DBits
	);
}

ExiError ExiError::HeaderVer() noexcept {
	return FROM_HEADER(kInvalidEXIHeader,
		.IHC = IHCType::kInvalidVersion,
		.Version = 0
	);
}

ExiError ExiError::HeaderVer(u64 Version) noexcept {
	if (Version == 0) {
		// Preview version.
		return ExiError::HeaderVer();
	} else if (!CheckIntCast<u16>(Version)) {
		// This path is pretty unlikely lol
		return ExiError::HeaderVer(exi::max_v<u16>);
	}
	return FROM_HEADER(kInvalidEXIHeader,
		.IHC = IHCType::kInvalidVersion,
		.Version = u16(Version)
	);
}

ExiError ExiError::HeaderAlign(AlignKind A, bool Compress) noexcept {
	return FROM_HEADER(kHeaderOptionsMismatch,
		.IHC = IHCType::kMixedAlignment,
		.Align = PackAlign(A, Compress)
	);
}

ExiError ExiError::HeaderStrict(Preserve Opt) noexcept {
	return FROM_HEADER(kHeaderOptionsMismatch,
		.IHC = IHCType::kStrictPreserved,
		.StrictOpts = Opt.get()
	);
}

ExiError ExiError::HeaderSelfContained(AlignKind A, bool Strict) noexcept {
	const bool Compress = (A == AlignKind::None); 
	return FROM_HEADER(kHeaderOptionsMismatch,
		.IHC = IHCType::kSelfContained,
		.SelfContained = PackAlign(A, Compress, Strict)
	);
}

ExiError ExiError::HeaderOutOfBand() noexcept {
	return FROM_HEADER(kHeaderOptionsMismatch,
		.IHC = IHCType::kOutOfBandOpts,
		.Default = 0
	);
}

static void FormatInvalidHeader(raw_ostream& OS, u32 Raw) {
	using enum InvalidHeaderCode;
	const HeaderExtra Ex = ToHeader(Raw);
	switch (Ex.IHC) {
	case kCookie:
		if (Ex.Cookie != '\0') {
			OS << "incorrect character in cookie '" << Ex.Cookie << "'";
		} else {
			OS << "invalid cookie";
		}
		return;
	case kDistinguishingBits:
		if EXI_UNLIKELY(Ex.Bits == 0b10) {
			LOG_WARN("Use of 'kDistinguishingBits' with valid sequence.");
			OS << "??? valid distinguishing bits";
			return;
		}
		OS << "invalid distinguishing bits";
		if (Ex.Bits != exi::max_v<u16>)
			OS << format(" '0b{:02b}'", Ex.Bits);
		return;
	case kInvalidVersion:
		if EXI_UNLIKELY(Ex.Version == 1) {
			LOG_WARN("Use of 'kInvalidVersion' with valid version.");
			OS << "??? supported Final Version " << Ex.Version;
			return;
		}
		if (Ex.Version != 0) {
			OS << "unsupported Final Version: " << Ex.Version;
		} else {
			OS << "invalid Preview Version";
		}
	default:
		return;
	}
}

#define ADD_NAME(NAME) 																												\
if (Opts.has(Preserve::NAME))																									\
	NameVec.emplace_back(#NAME, sizeof(#NAME));

static void FormatMismatchStrict(raw_ostream& OS, PreserveKind O) {
	Preserve Opts(O);
	Opts.unset(Preserve::Strict);
	
	SmallVec<StrRef> NameVec;
	ADD_NAME(Comments);
	ADD_NAME(DTDs);
	ADD_NAME(LexicalValues);
	ADD_NAME(PIs);
	ADD_NAME(Prefixes);

	if EXI_UNLIKELY(NameVec.empty()) {
		LOG_WARN("Use of 'kStrictPreserved' with valid options.");
		OS << "??? valid Preserve options";
		return;
	}

	OS << "using Preserve.";
	if (NameVec.size() == 1) {
		OS << NameVec.back();
	} else {
		OS << "{";
		for (StrRef Name : ArrayRef(NameVec).drop_back()) {
			OS << Name << ", ";
		}
		OS << NameVec.back() << "}";
	}
	OS << " in STRICT mode";
}

static void FormatMismatch(raw_ostream& OS, u32 Raw) {
	using enum InvalidHeaderCode;
	const HeaderExtra Ex = ToHeader(Raw);
	switch (Ex.IHC) {		
	case kMixedAlignment: // TODO
		OS << "mixing alignment and compression options";
		return;
	case kStrictPreserved:
		return FormatMismatchStrict(OS, Ex.StrictOpts);
	case kSelfContained: {
		auto SC = Ex.SelfContained;
		OS << "SelfContained was enabled ";
		if (SC.Strict) {
			OS << "in STRICT mode";
		} else if (SC.Compress) {
			OS << "with compression";
		} else if (SC.AlignV == u16(AlignKind::PreCompression)) {
			OS << "with pre-compression";
		} else {
			LOG_WARN("Use of 'kSelfContained' with valid options.");
			OS << " ??? with valid options";
		}
		return;
	}
	// case kDatatypeMap:
	case kOutOfBandOpts:
		OS << "prescence bit was not set, but no out-of-band options were provided";
		return;
	default:
		return;
	}
}

//////////////////////////////////////////////////////////////////////////
// BufferEndReached

ExiError ExiError::Full(i64 Bits) noexcept {
	if EXI_UNLIKELY(!CheckIntCast<u32>(Bits))
		return ExiError::Full();
	return ExiError(kBufferEndReached, static_cast<u32>(Bits));
}

static void FormatBufferEndReached(raw_ostream& OS, u32 Bits) {
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

//////////////////////////////////////////////////////////////////////////
// Frontend

bool ExiError::isSpecialCase() const {
	switch (this->EC) {
	case kInvalidEXIHeader:
	case kHeaderOptionsMismatch:
		return ToHeader(Extra).IHC != IHCType::kDefault;
	case kBufferEndReached:
		return Extra != ExiError::Unset;
	case kUnimplemented: // TODO
	default:
		return false;
	}
}

void ExiError::print(raw_ostream& OS) const {
	OS << "EXI Error: ";
	switch (this->EC) {
	case kInvalidEXIHeader:
		OS << "Invalid Header, ";
		return FormatInvalidHeader(OS, Extra);
	case kHeaderOptionsMismatch:
		OS << "Header Option Mismatch, ";
		return FormatMismatch(OS, Extra);
	case kBufferEndReached:
		return FormatBufferEndReached(OS, Extra);
	default:
		OS << get_error_message_what(EC);
	}
}

void ExiError::toVector(SmallVecImpl<char>& Vec) const {
	raw_svector_ostream OS(Vec);
  this->print(OS);
}

StrRef ExiError::msg(SmallVecImpl<char>& Vec) const {
	// if (not this->isSpecialCase())
	// 	return exi::get_error_message(EC);
	toVector(Vec);
	return StrRef(Vec.begin(), Vec.size());
}
