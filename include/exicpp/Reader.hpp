//===- exicpp/Reader.hpp --------------------------------------------===//
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

#pragma once

#ifndef EXIP_READER_HPP
#define EXIP_READER_HPP

#include "Basic.hpp"
#include "BinaryBuffer.hpp"
#include "Content.hpp"
#include <exip/EXIParser.h>
// #include <exip/grammarGenerator.h>

namespace exi {

using CParser = exip::Parser;

class Parser : protected CParser {
  Parser() noexcept : CParser() {}
  void(*shutdown)(CParser*) = nullptr;
public:
  ~Parser() { this->deinit(); }
public:
  template <class Source>
  [[nodiscard]]
  static Parser New(Source& appData, const StackBuffer& buf) {
    Parser parser {};
    if (Error E = parser.init(&buf, &appData)) {
      // TODO: Return Result<Parser, Error>
#if EXICPP_DEBUG && 0
      std::printf("\u001b[31;1m");
      std::printf("Error initializing: %s\n", E.message().data());
      std::printf("\u001b[0m");
      std::fflush(stdout);
#endif
    }
    ContentHandler::SetContent(parser.handler, &appData);
    return parser;
  }

public:
  /// @brief Parses the EXI header.
  /// @return An `Error` on failure.
  Error parseHeader(bool outOfBandOpts = false) {
    const CErrCode ret = exip::parseHeader(
      this, exip::boolean(outOfBandOpts));
    // Fill with the default schema.
    (void) this->setSchema();
    SET_PRESERVED(this->strm.header.opts.preserve, PRESERVE_PREFIXES);
    return Error::From(ErrCode(ret));
  }

  /// @brief Sets the schema.
  /// @return An `Error` on failure.
  Error setSchema(exip::EXIPSchema* schema = nullptr) {
    const CErrCode ret = exip::setSchema(this, schema);
    return Error::From(ErrCode(ret));
  }

  /// @brief Parses the next item.
  /// @return `ErrCode::Ok` to continue,
  /// `ErrCode::ParsingComplete` when finished.
  [[nodiscard]] ErrCode parseNext() {
    const CErrCode ret = exip::parseNext(this);
    return ErrCode(ret);
  }

  /// @brief Parses all the EXI items.
  /// @return An `Error` on failure.
  Error parseAll() {
    ErrCode lasterr = ErrCode::Ok;
    while (lasterr == ErrCode::Ok) {
      lasterr = this->parseNext();
    }
    if (lasterr != ErrCode::ParsingComplete)
      return Error::From(lasterr);
    return Error::Ok();
  }

private:
  Error init(const CBinaryBuffer* buf, void* appData) {
    auto* const parser = static_cast<CParser*>(this);
    CErrCode ret = exip::initParser(parser, *buf, appData);
    if (ret != CErrCode::EXIP_OK) {
      const auto err = ErrCode(ret);
      return Error::From(err);
    }
    return Error::Ok();
  }

  void deinit() {
    exip::destroyParser(this);
    if (CParser::strm.schema) {
      // TODO: Fix schemas
      // exip::destroySchema(CParser::strm.schema);
    }
    if (shutdown)
      shutdown(this);
  }
};

//======================================================================//
// XMLReader
//======================================================================//

} // namespace exi

#endif // EXIP_READER_HPP
