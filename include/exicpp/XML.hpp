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

#ifndef EXICPP_XML_HPP
#define EXICPP_XML_HPP

#include "Basic.hpp"
#include "Filesystem.hpp"
#include "HeapBuffer.hpp"
#include <rapidxml.hpp>

namespace exi {

using XMLPool = rapidxml::memory_pool<Char>;
using XMLDocument = rapidxml::xml_document<Char>;
using XMLAttribute = rapidxml::xml_attribute<Char>;
using XMLBase = rapidxml::xml_base<Char>;
using XMLNode = rapidxml::xml_node<Char>;
using XMLType = rapidxml::node_type;

bool set_xml_allocators(XMLDocument* doc);

class BoundDocument {
public:
  BoundDocument() : buf() {
    this->doc = std::make_unique<XMLDocument>();
    this->setAllocators();
  }
  BoundDocument(BoundDocument&&) = default;
public:
  static BoundDocument From(const fs::path& filename);

  template <int Flags = 0, bool DoTrim = true>
  static BoundDocument ParseFrom(const fs::path& filename) {
    constexpr int DefaultFlags =
        rapidxml::parse_no_string_terminators
      | rapidxml::parse_trim_whitespace;
    auto res = BoundDocument::From(filename);
    if (res) {
      Char* bufdata = res.buf.data();
      try {
        constexpr int DFlags = (DoTrim ? DefaultFlags : 0);
        res.doc->parse<Flags | DFlags>(bufdata);
      } catch (const std::exception& e) {
        BoundDocument::LogException(e);
        res.buf.reset();
      }
    }
    return res;
  }

public:
  XMLDocument* document() { return doc.get(); }
  const XMLDocument* document() const { return doc.get(); }

  explicit operator bool() const {
    return doc.get() && buf.data();
  }

private:
  static void LogException(const std::exception& e);
  void setAllocators();

private:
  Box<XMLDocument> doc;
  HeapBuffer buf;
};

} // namespace exi

#endif // EXICPP_XML_HPP
