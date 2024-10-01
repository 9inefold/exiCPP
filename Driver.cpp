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
#include <exip/EXISerializer.h>
#include <rapidxml.hpp>

namespace exi {

using CParser = exip::Parser;

class Parser : protected CParser {
  using BBType = BinaryBufferType;
  Parser() noexcept : CParser() {}
  void(*shutdown)(CParser*) = nullptr;
public:
  ~Parser() { this->deinit(); }
public:
  template <class Source>
  [[nodiscard]]
  static Parser New(Source& appData, const StackBuffer& buf) {
    Parser parser {};
    parser.init(&buf, &appData);
    ContentHandler::SetContent(parser.handler, &appData);
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

struct Example {
  unsigned elementCount = 0;
  unsigned nestingLevel = 0;

public:
  using enum exi::ErrCode;

  exi::ErrCode startDocument() const {
    std::cout << "Start: " << this << '\n';
    return Ok;
  }

  exi::ErrCode endDocument() const {
    std::cout << "End: " << this << '\n';
    return Ok;
  }
};

//======================================================================//
// rapidxml
//======================================================================//

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using Str = std::string;
using BoxedStr = std::unique_ptr<Str>;

namespace ansi {
  struct AnsiBase {
    std::string_view color {};
    friend std::ostream& operator<<(std::ostream& os, const AnsiBase& c) {
      if (!c.color.empty())
        return os << c.color;
      return os;
    }
  };


#ifndef DISABLE_ANSI
  inline constexpr AnsiBase reset   {"\u001b[0m"};
  inline constexpr AnsiBase red     {"\u001b[31;1m"};
  inline constexpr AnsiBase green   {"\u001b[32;1m"};
  inline constexpr AnsiBase blue    {"\u001b[34;1m"};
  inline constexpr AnsiBase yellow  {"\u001b[33;1m"};
  inline constexpr AnsiBase cyan    {"\u001b[36;1m"};
  inline constexpr AnsiBase white   {"\u001b[37;1m"};
#else
  inline constexpr AnsiBase reset   {};
  inline constexpr AnsiBase red     {};
  inline constexpr AnsiBase green   {};
  inline constexpr AnsiBase blue    {};
  inline constexpr AnsiBase yellow  {};
  inline constexpr AnsiBase cyan    {};
  inline constexpr AnsiBase white   {};
#endif
}

BoxedStr read_file(fs::path filepath) {
  if(filepath.is_relative()) {
    filepath = (fs::current_path() / filepath);
  }
  std::ifstream is ( filepath, std::ios::binary );
  is.unsetf(std::ios::skipws);
  if(!is) return BoxedStr();

  std::streampos size;
  is.seekg(0, std::ios::end);
  size = is.tellg();
  is.seekg(0, std::ios::beg);

  Str file_data;
  std::istream_iterator<char> start(is), end;
  file_data.reserve(size);
  file_data.insert(file_data.cbegin(), start, end);
  return std::make_unique<Str>(std::move(file_data));
}

/////////////////////////////////////////////////////////////////////////

#include <cassert>

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

exi::StrRef get_name(rapidxml::xml_base<>* data) {
  if (!data || data->name_size() == 0) return "";
  return {data->name(), data->name_size()};
}

exi::StrRef get_value(rapidxml::xml_base<>* data) {
  if (!data || data->value_size() == 0) return "";
  return {data->value(), data->value_size()};
}

void iter_nodes(rapidxml::xml_document<>* pnode, std::size_t starting_depth = 0) {
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

void recurse_nodes(rapidxml::xml_node<>* pnode, std::size_t depth = 0) {
  if EXICPP_UNLIKELY(!pnode->parent()) {
    return recurse_nodes(pnode->first_node(), depth);
  }

  std::string padding(depth, ' ');
  for (auto* node = pnode; node; node = node->next_sibling()) {
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

    auto* first_node = node->first_node();
    if (first_node) {
      recurse_nodes(first_node, depth + 2);
    }
  }
}

template <bool UseRecursive = false>
void test_file(const fs::path& filepath) {
  auto file = read_file(filepath);
  if (!file) {
    std::cerr << "Unable to locate file "
      << filepath << "!\n";
    return;
  }

  using namespace rapidxml;
  xml_document<exi::Char> doc;
  doc.parse<0>(file->data());

  std::cout
    << ansi::red << '[' << (UseRecursive ? "recursive" : "iterative") << "] "
    << ansi::yellow << "In " << filepath << ':'
    << ansi::reset << '\n';

  if constexpr (UseRecursive) {
    recurse_nodes(&doc, 1);
  } else {
    iter_nodes(&doc, 1);
  }
  std::cout << std::endl;
}

int main() {
  std::cout << std::boolalpha;

  exi::Char stackData[512] {};
  exi::BinaryBuffer buf(stackData);
  Example appData {};
  auto parser = exi::Parser::New(appData, buf);
  // auto err = parser.parseHeader();
  // std::cout << exi::getErrString(err) << std::endl;

  // initParser(&parser, buffer.getCBuffer(), &data);
  // destroyParser(&parser);

  test_file("examples/Customers.xml");
  test_file("examples/Namespace.xml");
}
