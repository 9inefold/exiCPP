//===- exicpp/Errors.hpp --------------------------------------------===//
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
//
//  Safer wrapper over exip::errorCode.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_ERRORS_HPP
#define EXIP_ERRORS_HPP

#include "Basic.hpp"
#include "Traits.hpp"
#include <exip/errorHandle.h>

namespace exi {

using CErrCode = exip::errorCode;

enum class ErrCode {
	/** No error, everything is OK. */
	Ok                                          = 0,
	/** The code for this function
	 * is not yet implemented. */
	NotImplemented                              = 1,
	/** Any error that does not fall
	 * into the other categories */
	UnexpectedError                             = 2,
	/** Hash table error  */
	HashTableError                              = 3,
	/** Array out of bound  */
	OutOfBoundBuffer                            = 4,
	/** Try to access null pointer */
	NullPointerRef                              = 5,
	/** Unsuccessful memory allocation */
	MemoryAllocationError                       = 6,
	/** Error in the EXI header */
	InvalidEXIHeader                            = 7,
	/** Processor state is inconsistent
	 * with the stream events  */
	InconsistentProcState                       = 8,
	/** Received EXI value type or event
	 * encoding that is invalid according
	 * to the specification */
	InvalidEXIInput                             = 9,
	/** Buffer end reached  */
	BufferEndReached                            = 10,
	/** ...Parsing complete  */
	ParsingComplete                             = 11,
  /** The information passed to
   * the EXIP API is invalid */
	InvalidConfig                               = 12,
  /** When encoding XML Schema in EXI the prefixes must be preserved:
   * When qualified namesNS are used in the values of AT or CH events in an EXI Stream,
   * the Preserve.prefixes fidelity option SHOULD be turned on to enable the preservation of
   * the NS prefix declarations used by these values. Note, in particular among other cases,
   * that this practice applies to the use of xsi:type attributes in EXI streams when Preserve.lexicalValues
   * fidelity option is set to true. */
	NoPrefixesPreservedXMLSchema                = 13,
	InvalidStringOp                             = 14,
	/** Mismatch in the header options.
	 * This error can be due to:
	 * 1) The "alignment" element MUST NOT appear in an EXI options document when the "compression" element is present;
	 * 2) The "strict" element MUST NOT appear in an EXI options document when one of "dtd", "prefixes",
	 * "comments", "pis" or "selfContained" element is present in the same options document. That is only the
	 * element "lexicalValues", from the fidelity options, is permitted to occur in the presence of "strict" element;
	 * 3) The "selfContained" element MUST NOT appear in an EXI options document when one of "compression",
	 * "pre-compression" or "strict" elements are present in the same options document.
	 * 4) The  datatypeRepresentationMap option does not take effect when the value of the Preserve.lexicalValues
	 * fidelity option is true (see 6.3 Fidelity Options), or when the EXI stream is a schema-less EXI stream.
	 * 5) Presence Bit for EXI Options not set and no out-of-band options set
	 */
	HeaderOptionsMismatch                       = 15,
	/**
	 * Send a signal to the EXIP parser from a content handler callback
	 * for gracefully stopping the EXI stream parsing.
	 */
	Stop                                        = 16,
	HandlerStop                                 = Stop,
  LastValue                                   = Stop,
};

inline StrRef getErrString(ErrCode err) {
#if EXIP_DEBUG == ON
  const auto idx = std::size_t(err);
  if EXICPP_LIKELY(idx <= std::size_t(ErrCode::LastValue))
    return exip::errorCodeStrings[idx];
#endif
  return "";
}

//======================================================================//
// Error
//======================================================================//

class [[nodiscard]] Error {
	using MsgType = const char*;

	enum State : std::uint8_t {
		NoError,
		Unowned,
		Owned,
		Invalid,
	};

	constexpr Error() = default;
	Error(MsgType msg, State state = Unowned) :
	 msg(msg), state(state) {
	}

public:
	Error(const Error&) = delete;
	Error(Error&& e) : Error(e.msg, e.state) {
		e.clear();
	}

	Error& operator=(const Error&) = delete;
	Error& operator=(Error&& e);
	~Error() { this->clear(); }

	static Error Ok() { return Error(); }
	static Error From(StrRef msg, bool clone = false);
	static Error From(ErrCode err);
	static Error MakeOwned(StrRef msg);

public:
	void clear();
	StrRef message() const;

	explicit operator bool() const {
		return (state != NoError);
	}

private:
	MsgType msg = nullptr;
	State state = NoError;
};

} // namespace exi

#endif // EXIP_ERRORS_HPP
