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
  using enum exi::ErrCode;

  exi::ErrCode startDocument() const {
    std::cout << "Beg: " << this << '\n';
    return Ok;
  }

  exi::ErrCode endDocument() const {
    std::cout << "End: " << this << '\n';
    return Ok;
  }

  exi::ErrCode startElement(exi::QName name) {
    std::cout << "#" << elementCount << ": "
      << name << '\n';
    ++this->elementCount;
    ++this->nestingLevel;
    return Ok;
  }

  exi::ErrCode endElement() {
    --this->nestingLevel;
    return Ok;
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

[[noreturn]] void flushing_assert(
 const char* message, const char* file, unsigned line) {
  std::fflush(stdout);
  _assert(message, file, line);
  __builtin_unreachable();
}

#ifdef NDEBUG
# define my_assert(expr) ((void)0)
#else
# define my_assert(expr) \
  ((void)((static_cast<bool>((expr))) || \
   (flushing_assert(#expr,__FILE__,__LINE__), false)))
#endif /* !defined (NDEBUG) */

exi::StrRef get_name(exi::XMLBase* data) {
  if (!data || data->name_size() == 0) return "";
  return {data->name(), data->name_size()};
}

exi::StrRef get_value(exi::XMLBase* data) {
  if (!data || data->value_size() == 0) return "";
  return {data->value(), data->value_size()};
}

void iter_nodes(exi::XMLDocument* pnode, std::size_t starting_depth = 0) {
  using namespace rapidxml;
  std::size_t depth = starting_depth;
  std::string padding(starting_depth, ' ');

  xml_node<>* curr_node = pnode;
  xml_node<>* last_node = nullptr;
  auto& node = curr_node;

  auto next = [&]() -> bool {
    last_node = curr_node;
    if (curr_node->first_node()) {
      curr_node = curr_node->first_node();
      depth += 2;
      padding.resize(depth, ' ');
      return true;
    }

    auto* parent = curr_node->parent();
    if (!parent) return false;

    if (curr_node->next_sibling()) {
      curr_node = curr_node->next_sibling();
      return true;
    }

    curr_node = parent;
    parent = curr_node->parent();

    while (parent) {
      depth -= 2;
      padding.resize(depth);

      if (curr_node->next_sibling()) {
        curr_node = curr_node->next_sibling();
        return true;
      }

      curr_node = parent;
      parent = curr_node->parent();
    }

    return false;
  };

  while (next()) {
    if (node->type() == rapidxml::node_data) {
      std::cout << padding << '[' << get_value(node) << "]\n";
      continue;
    }

    std::cout << padding << get_name(node) << ":\n";

    for (auto* attr = node->first_attribute();
      attr; attr = attr->next_attribute())
    {
      std::cout << padding << " {"
        << ansi::red    << get_name(attr) 
        << ansi::reset  << '='
        << ansi::cyan   << get_value(attr) 
        << ansi::reset  << "}\n";
    }
  }
}

/////////////////////////////////////////////////////////////////////////

bool write_file(const fs::path& path, const fs::path& outpath) {
  using namespace exi;
  auto filepath = path.string();
  auto xmldoc = BoundDocument::ParseFrom(filepath);
  if (!xmldoc) {
    std::cout 
      << ansi::red << "Unable to locate file "
      << path.filename() << "!\n" << ansi::reset;
    return false;
  }

  auto exiFile = outpath.string();
  InlineStackBuffer<512> buf;
  if (Error E = buf.writeFile(exiFile)) {
    std::cout 
      << ansi::red << "Error in '" << exiFile << "': " << E.message()
      << ansi::reset << std::endl;
    return false;
  }

  return true;
}

bool read_file(const fs::path& outpath) {
  using namespace exi;
  auto exiFile = outpath.string();
  

  // rapidxml::print(std::cout, *xmldoc.document());
  // std::cout << std::endl;
  return true;
}

void test_file(exi::StrRef filepath) {
  fs::path path = fs::absolute(filepath);
  fs::path outpath = path.replace_extension("exi");

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

  test_file("examples/Customers.xm");
  // test_file("examples/Namespace.xml");
}
