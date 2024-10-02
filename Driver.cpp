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

#include <exicpp/Reader.hpp>
#include <exicpp/Writer.hpp>
#include <exicpp/XML.hpp>
#include <exip/EXISerializer.h>

#include <iostream>

inline std::ostream& operator<<(std::ostream& os, const exi::QName& name) {
  const auto prefix = name.prefix();
  if (prefix.empty())
    return os << name.localName();
  return os << prefix << ':' << name.localName();
}

struct Example {
  unsigned elementCount = 0;
  unsigned nestingLevel = 0;
public:
  using ErrCode = exi::ErrCode;

  ErrCode startDocument() const {
    std::cout << "Beg: " << this << '\n';
    return ErrCode::Ok;
  }

  ErrCode endDocument() const {
    std::cout << "End: " << this << '\n';
    return ErrCode::Ok;
  }

  ErrCode startElement(exi::QName name) {
    std::cout << "#" << elementCount << ": "
      << name << '\n';
    ++this->elementCount;
    ++this->nestingLevel;
    return ErrCode::Ok;
  }

  exi::ErrCode endElement() {
    --this->nestingLevel;
    return ErrCode::Ok;
  }
};

//======================================================================//
// rapidxml
//======================================================================//

#include <filesystem>

namespace fs = std::filesystem;

namespace ansi {
struct AnsiBase {
  std::string_view color {};
  friend std::ostream& operator<<(std::ostream& os, const AnsiBase& c) {
    if (!c.color.empty())
      return os << c.color;
    return os;
  }
};

#if DISABLE_ANSI
# define DECL_ANSI(name, val) \
  inline constexpr AnsiBase name {}
#else
# define DECL_ANSI(name, val) \
  inline constexpr AnsiBase name {val}
#endif

DECL_ANSI(reset, "\u001b[0m");
DECL_ANSI(red,   "\u001b[31;1m");
DECL_ANSI(green, "\u001b[32;1m");
DECL_ANSI(blue,  "\u001b[34;1m");
DECL_ANSI(yellow,"\u001b[33;1m");
DECL_ANSI(cyan,  "\u001b[36;1m");
DECL_ANSI(white, "\u001b[37;1m");

#undef DECL_ANSI
}

/////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <rapidxml_print.hpp>

bool write_file(const std::string& path, const std::string& outpath) {
  using namespace exi;
  auto xmldoc = BoundDocument::ParseFrom(path);
  if (!xmldoc) {
    std::cout 
      << ansi::red << "Unable to locate file "
      << path << "!\n" << ansi::reset;
    return false;
  }

  InlineStackBuffer<512> buf;
  if (Error E = buf.writeFile(outpath)) {
    std::cout 
      << ansi::red << "Error in '" << outpath << "': " << E.message()
      << ansi::reset << std::endl;
    return false;
  }

  if (Error E = write_xml(xmldoc.document(), buf)) {
    std::cout 
      << ansi::red << "Serialization error: " << E.message()
      << ansi::reset << std::endl;
    return false;
  }

  return true;
}

bool read_file(const std::string& outpath) {
  using namespace exi;

  InlineStackBuffer<512> buf;
  if (Error E = buf.readFile(outpath)) {
    std::cout 
      << ansi::red << "Error in '" << outpath << "': " << E.message()
      << ansi::reset << std::endl;
    return false;
  }

  // TODO: Add parser
  // rapidxml::print(std::cout, *xmldoc.document());
  // std::cout << std::endl;
  return true;
}

void test_file(exi::StrRef filepath) {
  std::string path = std::string(filepath) + ".xml";
  std::string outpath = std::string(filepath) + ".exi";

  if (!write_file(path, outpath))
    return;
  read_file(outpath);
}

int main() {
  using namespace exi;
  using exi::Error;
  using exi::ErrCode;

  InlineStackBuffer<512> buf;
  StrRef filename = "vendored/exip/examples/simpleDecoding/exipd-test.exi";
  if (Error E = buf.readFile(filename)) {
    std::cout 
      << ansi::red << "Error in '" << filename << "': " << E.message()
      << ansi::reset << std::endl;
  }

  Example appData {};
  auto parser = exi::Parser::New(appData, buf);

  if (Error E = parser.parseHeader()) {
    std::cout
      << ansi::red << "Header error: " << E.message()
      << ansi::reset << std::endl;
  }

  if (Error E = parser.parseAll()) {
    std::cout
      << ansi::red << "Parse error: " << E.message()
      << ansi::reset << std::endl;
  }

  test_file("examples/Basic");
  // test_file("examples/Namespace.xml");
}
