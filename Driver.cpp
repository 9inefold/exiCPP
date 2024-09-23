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

#include <exicpp/Content.hpp>
#include <exicpp/BinaryBuffer.hpp>
#include <exip/EXIParser.h>

namespace exi {

using CParser = exip::Parser;
using CContentHandler = exip::ContentHandler;

//======================================================================//
// Parser
//======================================================================//

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

  template <typename Source, typename AppData>
  static CErrCode endElement(void* appData) {
    const ErrCode ret = Source::endElement(
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  template <typename Source, typename AppData>
  static CErrCode attribute(CQName qname, void* appData) {
    const ErrCode ret = Source::attribute(
      QName(qname),
      static_cast<AppData*>(appData)
    );
    return CErrCode(ret);
  }

  ////////////////////////////////////////////////////////////////////////

  template <class Source, typename AppData>
  static void SetContent(CContentHandler& handler, AppData* data) {
    REQUIRES_FUNC(startDocument, (), (data)) {
      handler.startDocument = &Parser::startDocument<Source, AppData>;
    }
    REQUIRES_FUNC(endDocument, (), (data)) {
      handler.endDocument = &Parser::endDocument<Source, AppData>;
    }
    REQUIRES_FUNC(startElement, (QName qname), (qname, data)) {
      handler.startElement = &Parser::startElement<Source, AppData>;
    }
    REQUIRES_FUNC(endElement, (QName qname), (qname, data)) {
      handler.endElement = &Parser::endElement<Source, AppData>;
    }
    REQUIRES_FUNC(attribute, (QName qname), (qname, data)) {
      handler.attribute = &Parser::attribute<Source, AppData>;
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
