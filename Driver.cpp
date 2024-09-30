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
  template <class Source, typename AppData>
  [[nodiscard]]
  static Parser New(const StackBuffer& buf, AppData* appData) {
    Parser parser {};
    parser.init(&buf, appData);
    ContentHandler::SetContent<Source>(parser.handler, appData);
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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using Str = std::string;
using BoxedStr = std::unique_ptr<Str>;

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

void iter_nodes(rapidxml::xml_node<>* pnode, std::size_t depth = 0) {
  std::string padding(depth, ' ');
  for (auto* node = pnode; node; node = node->next_sibling()) {
    if (node->type() == rapidxml::node_data) {
      exi::StrRef val(node->value(), node->value_size());
      std::cout << padding << '[' << val << "]\n";
      continue;
    }

    exi::StrRef name(node->name(), node->name_size());
    std::cout << padding << name << ":\n";

    for (auto* attr = node->first_attribute();
      attr; attr = attr->next_attribute())
    {
      exi::StrRef attr_name(attr->name(), attr->name_size());
      exi::StrRef attr_val(attr->value(), attr->value_size());
      std::cout << padding << " {" << name
        << attr_name << '=' << attr->value() << "}\n";
    }

    auto* first_node = node->first_node();
    if (first_node) {
      iter_nodes(first_node, depth + 2);
    }
  }
}

int main() {
  exi::Char stackData[512] {};
  exi::BinaryBuffer buf(stackData);
  AppData appData {};
  auto parser = exi::Parser::New<Example>(buf, &appData);
  // auto err = parser.parseHeader();
  // std::cout << exi::getErrString(err) << std::endl;

  // initParser(&parser, buffer.getCBuffer(), &data);
  // destroyParser(&parser);

  auto file = read_file("examples/Customers.xml");
  if (!file) {
    std::cerr << "Unable to locate file!\n";
    return 1;
  }

  using namespace rapidxml;
  xml_document<exi::Char> doc;
  doc.parse<0>(file->data());

  auto* first_node = doc.first_node();
  iter_nodes(first_node);
}
