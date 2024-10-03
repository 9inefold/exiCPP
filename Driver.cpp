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

#define NFORMAT
#include <exicpp/Debug/Format.hpp>
#include <iostream>

inline std::ostream& operator<<(std::ostream& os, const exi::QName& name) {
  const auto prefix = name.prefix();
  if (prefix.empty())
    return os << name.localName();
  return os << prefix << ':' << name.localName();
}

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

DECL_ANSI(reset, "\033[0m");
DECL_ANSI(red,   "\033[31;1m");
DECL_ANSI(green, "\033[32;1m");
DECL_ANSI(blue,  "\033[34;1m");
DECL_ANSI(yellow,"\033[33;1m");
DECL_ANSI(cyan,  "\033[36;1m");
DECL_ANSI(white, "\033[37;1m");

#undef DECL_ANSI

struct AnsiEnd {};
inline constexpr AnsiEnd endl {};

inline std::ostream& operator<<(std::ostream& os, const AnsiEnd&) {
#if DISABLE_ANSI
  return os << '\n';
#else
  return os << ansi::reset << '\n';
#endif
}

}

struct Example {
  using ErrCode = exi::ErrCode;

  enum Ty {
    DOCTYPE,
    ELEMENT,
    DATA,
    ATTRIBUTE,
    ATTRIBUTE_DATA,
    NONE,
  };

public:
  constexpr Example() = default;

  ErrCode startDocument() {
    LOG_INFO("");
    this->lastType = DOCTYPE;
    return ErrCode::Ok;
  }

  ErrCode endDocument() {
    LOG_INFO("");
    this->lastType = NONE;
    return ErrCode::Ok;
  }

  ErrCode startElement(const exi::QName& name) {
    if (lastType == ATTRIBUTE) {
      LOG_INFO("Attr");
      outs() << ansi::yellow
        << " > " << name;
#ifndef NFORMAT
      outs(false) << ansi::endl;
#endif
      this->lastType = ATTRIBUTE_DATA;
      return ErrCode::Ok; 
    } else if (lastType == ATTRIBUTE_DATA) {
      LOG_INFO("AttrData");
      return ErrCode::Ok; 
    }

    LOG_INFO("");

    if (name.localName().empty()) {
      this->lastType = DATA;
      return ErrCode::Ok;
    }

    outs() << ansi::red
      << "#" << elementCount << ": "
      << name << ansi::endl;
    ++this->elementCount;
    ++this->nestingLevel;
    return ErrCode::Ok;
  }

  exi::ErrCode endElement() {
    if (lastType == ATTRIBUTE) {
      LOG_INFO("Attr");
      this->lastType = ELEMENT;
      return ErrCode::Ok;
    } else if (lastType == ATTRIBUTE_DATA) {
      LOG_INFO("AttrData");
      this->lastType = ATTRIBUTE;
      return ErrCode::Ok;
    }

    if (lastType == DATA) {
      LOG_INFO("Data");
      this->lastType = ELEMENT;
      return ErrCode::Ok;
    }

    if (nestingLevel > 0) {
      LOG_INFO("");
      --this->nestingLevel;
    } else {
      LOG_ERR("INVALID NESTING LEVEL");
    }
    outs()
      << ansi::blue << "END!"
      << ansi::endl;
    return ErrCode::Ok;
  }

  exi::ErrCode attribute(const exi::QName& name) {
    LOG_INFO("");
    this->lastType = ATTRIBUTE;
    return ErrCode::Ok;
  }

  exi::ErrCode stringData(exi::StrRef str) {
    if (lastType == ATTRIBUTE) {
      LOG_INFO("Attr");
      return ErrCode::Ok;
    } else if (lastType == ATTRIBUTE_DATA) {
      LOG_INFO("AttrData");
#ifndef NFORMAT
      outs()
        << ansi::yellow << " = "
#else
      outs(false) << "="
#endif
        << str << ansi::endl;
      return ErrCode::Ok;
    }

    if (lastType == DATA)
      LOG_INFO("Data");
    else
      LOG_INFO("");

    outs()
      << ansi::white << " = "
      << str << ansi::endl;
    return ErrCode::Ok;
  }

private:
  std::ostream& outs(bool printDepth = true) {
    if (!printDepth)
      return std::cout;
    return std::cout << std::string(nestingLevel * 2, ' ');
  }

private:
  unsigned elementCount = 0;
  unsigned nestingLevel = 0;
  Ty lastType = NONE;
};

/////////////////////////////////////////////////////////////////////////

struct XMLBuilder {
  using Ch = exi::Char;
  XMLBuilder() : doc(), node(doc.document()) {}
public:

private:


private:
  exi::XMLDocument doc;
  exi::XMLNode* node = nullptr;
};

bool write_file(const std::string& path, const std::string& outpath);
bool read_file(const std::string& outpath);
void test_file(exi::StrRef filepath);
void test_exi(exi::StrRef file);

int main() {
  // test_exi("vendored/exip/examples/simpleDecoding/exipd-test.exi");
  test_file("examples/Basic2");
  // test_file("examples/Basic");
  // test_file("examples/Customers");
  // test_file("examples/Namespace.xml");
}

#include <cassert>
#include <rapidxml_print.hpp>
#include <fmt/color.h>

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
    // std::cout 
    //   << ansi::red << "Serialization error: " << E.message()
    //   << ansi::reset << std::endl;
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

exi::StrRef file_folder() {
  constexpr auto& rawfile = __FILE__;
  constexpr exi::StrRef file {rawfile};
  constexpr std::size_t pos = file.find_last_of("\\/");
  if (pos == exi::StrRef::npos)
    return "";
  return file.substr(0, pos + 1);
}

std::string get_relative(exi::StrRef path) {
  return std::string(file_folder()) + std::string(path);
}

void test_file(exi::StrRef filepath) {
  const auto basepath = get_relative(filepath);
  std::string path = basepath + ".xml";
  std::string outpath = basepath + ".exi";

  if (!write_file(path, outpath))
    return;
  fmt::print(fmt::fg(fmt::color::blue_violet),
    "\n----------------------------------------------\n");
  test_exi(std::string(filepath) + ".exi");
  // read_file(outpath);
}

void test_exi(exi::StrRef file) {
  using namespace exi;
  InlineStackBuffer<512> buf;
  auto filename = get_relative(file);
  if (Error E = buf.readFile(filename)) {
    std::cout
      << ansi::red << "Error in '" << filename << "': " << E.message()
      << ansi::reset << std::endl;
    return;
  }

  Example appData {};
  auto parser = exi::Parser::New(appData, buf);

  if (Error E = parser.parseHeader()) {
    std::cout << '\n'
      << "In '" << filename << "': "
      << ansi::reset << '\n';
    return;
  }

  if (Error E = parser.parseAll()) {
    std::cout << '\n'
      << "In '" << filename << "': "
      << ansi::reset << '\n';
    return;
  }
  std::cout << std::endl;
}
