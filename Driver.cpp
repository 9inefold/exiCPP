//===- Driver.cpp ---------------------------------------------------===//
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

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <exip/EXIParser.h>

#define ALWAYS_INLINE [[gnu::always_inline]] inline

namespace exi {

using Char = CHAR_TYPE;
using StrRef = std::basic_string_view<Char>;

using CString = exip::String;
using CQName  = exip::QName;
using CBinaryBuffer = exip::BinaryBuffer;

using CParser = exip::Parser;
using CContentHandler = exip::ContentHandler;

//======================================================================//
// Error Codes
//======================================================================//

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
  LastValue = Stop,
};

ALWAYS_INLINE static StrRef getErrString(ErrCode err) {
#if EXIP_DEBUG == ON
  const auto idx = std::size_t(err);
  if (idx <= std::size_t(ErrCode::LastValue)) [[likely]]
    return exip::errorCodeStrings[idx];
#endif
  return "";
}

//======================================================================//
// String Manip
//======================================================================//

class QName : protected CQName {
  friend class Parser;
  constexpr QName() : CQName() {}
  constexpr QName(const CQName& name) : CQName(name) {}

  ALWAYS_INLINE static StrRef ToStr(const CString& str) {
    return StrRef(str.str, str.length);
  }

  inline static StrRef ToStr(const CString* str) {
    if (str) [[likely]]
      return QName::ToStr(*str);
    return "";
  }

public:
  StrRef uri()        const { return QName::ToStr(CQName::uri); }
  StrRef localName()  const { return QName::ToStr(CQName::localName); }
  StrRef prefix()     const { return QName::ToStr(CQName::prefix); }
};

//======================================================================//
// Binary Buffer
//======================================================================//

enum class BinaryBufferType {
  Stack,
  Vector,
  Unknown,
};

template <BinaryBufferType>
struct BinaryBuffer;

template <>
struct BinaryBuffer<BinaryBufferType::Stack> : protected CBinaryBuffer {
  friend class Parser;
  constexpr BinaryBuffer() = default;

  template <std::size_t N>
  ALWAYS_INLINE BinaryBuffer(Char(&data)[N]) :
   CBinaryBuffer{data, N, 0} {}

  template <std::size_t N>
  ALWAYS_INLINE void set(Char(&data)[N]) {
    return this->setInternal(data, N);
  }

private:
  void setInternal(Char* data, std::size_t len) {
    CBinaryBuffer::buf = data;
    CBinaryBuffer::bufLen = len;
    CBinaryBuffer::bufContent = 0;
  }
};

using StackBuffer = BinaryBuffer<BinaryBufferType::Stack>;

template <std::size_t N>
BinaryBuffer(Char(&)[N]) -> StackBuffer;

//======================================================================//
// Parser
//======================================================================//

#if __cpp_concepts >= 201907L
# define REQUIRES_FUNC(ty, fn, fn_args, call_args) \
  if constexpr (requires fn_args { \
    {ty::fn call_args} -> std::same_as<ErrCode>; \
  })
#else /* C++17 or lower */
# define REQUIRES_FUNC(...) \
  if constexpr (false)
#endif

class Parser : protected CParser {
  using BBType = BinaryBufferType;
  Parser() noexcept : CParser() {}
  void(*shutdown)(CParser*) = nullptr;
public:
  ~Parser() { this->deinit(); }

private:
  template <typename Source, typename AppData>
  static CErrCode startDocument(void* appData) {
    const ErrCode ret = Source::startDocument(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode endDocument(void* appData) {
    const ErrCode ret = Source::endDocument(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode startElement(CQName qname, void* appData) {
    const ErrCode ret = Source::startElement(
      QName(qname),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source, typename AppData>
  static void SetContent(CContentHandler& handler, AppData* data) {
    REQUIRES_FUNC(Source, startDocument, (), (data)) {
      handler.startDocument = &Parser::startDocument<Source, AppData>;
    }
    REQUIRES_FUNC(Source, endDocument, (), (data)) {
      handler.endDocument = &Parser::endDocument<Source, AppData>;
    }
    REQUIRES_FUNC(Source, startElement, (QName qname), (qname, data)) {
      handler.endDocument = &Parser::startElement<Source, AppData>;
    }
  }

public:
  template <class Source, typename AppData>
  [[nodiscard]]
  static Parser New(const StackBuffer& buf, AppData* appData) {
    Parser parser {};
    parser.init(&buf, appData);
    Parser::SetContent<Source>(parser.handler, appData);
    return parser;
  }

public:
  [[nodiscard]] ErrCode parseHeader(bool outOfBandOpts = false) {
    const CErrCode ret = exip::parseHeader(
      this, exip::boolean(outOfBandOpts));
    return ErrCode(ret);
  }

private:
  bool init(const CBinaryBuffer* buf, void* appData) {
    auto* const parser = static_cast<CParser*>(this);
    CErrCode err = exip::initParser(parser, *buf, appData);
    if (err != CErrCode::EXIP_OK) {
      return false;
    }
    return true;
  }

  void deinit() {
    exip::destroyParser(this);
    if (shutdown)
      shutdown(this);
  }
};

} // namespace exi

/////////////////////////////////////////////////////////////////////////

#include <iostream>

struct AppData {
  unsigned elementCount;
  unsigned nestingLevel;
};

struct Example {
  using enum exi::ErrCode;

  static exi::ErrCode startDocument(AppData* data) {
    std::cout << "Start: " << data << '\n';
    return Ok;
  }

  static exi::ErrCode endDocument(AppData* data) {
    std::cout << "End: " << data << '\n';
    return Ok;
  }
};

int main() {
  exi::Char stackData[512] {};
  exi::BinaryBuffer buf(stackData);
  AppData appData {};
  auto parser = exi::Parser::New<Example>(buf, &appData);

  auto err = parser.parseHeader();
  // std::cout << exi::getErrString(err) << std::endl;

  // initParser(&parser, buffer.getCBuffer(), &data);
  // destroyParser(&parser);
}
