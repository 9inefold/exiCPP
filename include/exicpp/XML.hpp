//===- exicpp/XML.hpp -----------------------------------------------===//
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
//  Defines aliases for the rapidxml types.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_XML_HPP
#define EXIP_XML_HPP

#include "Basic.hpp"
#include "HeapBuffer.hpp"
#include <rapidxml.hpp>

namespace exi {

using XMLDocument = rapidxml::xml_document<Char>;
using XMLBase = rapidxml::xml_base<Char>;
using XMLNode = rapidxml::xml_node<Char>;
using XMLType = rapidxml::node_type;

class BoundDocument {
public:
  BoundDocument() : buf() {
    this->doc = std::make_unique<XMLDocument>();
  }
  BoundDocument(BoundDocument&&) = default;
public:
  static BoundDocument From(StrRef filename);

  template <int Flags = 0>
  static BoundDocument ParseFrom(StrRef filename) {
    auto res = BoundDocument::From(filename);
    Char* bufdata = res.buf.data();
    if (bufdata)
      res.doc->parse<Flags>(bufdata);
    return res;
  }

public:
  XMLDocument* document() { return doc.get(); }
  const XMLDocument* document() const { return doc.get(); }

  explicit operator bool() const {
    return doc.get() && buf.data();
  }

private:
  Box<XMLDocument> doc;
  HeapBuffer buf;
};

} // namespace exi

#endif // EXIP_XML_HPP
