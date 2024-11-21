//===- utils/ExiToXml.cpp -------------------------------------------===//
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

#include "ExiToXml.hpp"
#include "STL.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <exicpp/BinaryBuffer.hpp>
#include <exicpp/Reader.hpp>
#include <exicpp/Debug/Format.hpp>

using namespace exi;

namespace {

struct InternRef : public StrRef {
  using BaseType = StrRef;
public:
  constexpr InternRef() noexcept = default;
  InternRef(Char* data, std::size_t len) noexcept : BaseType(data, len) { }
  constexpr InternRef(const InternRef&) noexcept = default;
  constexpr InternRef& operator=(const InternRef&) noexcept = default;
public:
  Char* data() const { return const_cast<Char*>(BaseType::data()); }
};

struct XMLBuilder {
  using Ty = XMLType;

  XMLBuilder() :
   doc(std::make_unique<XMLDocument>()),
   node(doc->document()) {
    set_xml_allocators(doc.get());
  }

  XMLDocument* document() { return doc.get(); }
  const XMLDocument* document() const { return doc.get(); }
  Box<XMLDocument> take() {
    return std::move(this->doc);
  }

public:
  ErrCode startDocument() {
    this->node = doc->document();
    LOG_ASSERT(node && node->type() == Ty::node_document);
    return ErrCode::Ok;
  }

  ErrCode endDocument() {
    LOG_ASSERT(node && node->type() == Ty::node_document);
    return ErrCode::Ok;
  }

  ErrCode startElement(const QName& name) {
    const auto ln = this->internQName(name);
    auto ty = ln.empty() ? Ty::node_data : Ty::node_element;
    auto* newNode = this->makeNode<true>(ty, ln);
    // Add as child
    node->append_node(newNode);
    this->node = newNode;
    return ErrCode::Ok;
  }

  ErrCode endElement() {
    LOG_ASSERT(node);
    this->node = node->parent();
    return ErrCode::Ok;
  }

  ErrCode namespaceDeclaration(
    StrRef ns,
    StrRef prefix,
    bool isLocal) 
  {
    if (isLocal && !prefix.empty()) {
      StrRef name(node->name(), node->name_size());
      auto fullName = fmt::format("{}:{}", prefix, name);
      InternRef iname = this->intern(fullName);
      node->name(iname.data(), iname.size());
    }

    auto fullPre = XMLBuilder::FormatNs(prefix);
    auto* attr = this->makeAttribute(fullPre, ns);
    node->append_attribute(attr);
    return ErrCode::Ok;
  }

  ErrCode attribute(const QName& name) {
    LOG_ASSERT(!this->attr);
    this->attr = this->makeAttribute(name.localName());
    node->append_attribute(attr);
    return ErrCode::Ok;
  }

  ErrCode stringData(StrRef str) {
    if (this->attr) {
      InternRef istr = this->intern(str);
      attr->value(istr.data(), istr.size());
      this->attr = nullptr;
      return ErrCode::Ok;
    }

    /*
    if EXICPP_UNLIKELY(node->type() != Ty::node_data) {
      // LOG_WARN("Expected 'node_data', got '{}'",
      //   unsigned(node->type()));
      fmt::println("[WARN] Expected 'node_data', got '{}'",
        unsigned(node->type()));
      return ErrCode::Ok;
    }
    */
    InternRef istr = this->intern(str);
    node->value(istr.data(), istr.size());
    return ErrCode::Ok;
  }

private:
  static std::string FormatNs(StrRef prefix) {
    if (prefix.empty())
      return "xmlns";
    return fmt::format("xmlns:{}", prefix);
  }

  InternRef internQName(const QName& qname) {
    const auto prefix = qname.prefix();
    if (prefix.empty()) {
      return this->intern(qname.localName());
    }
    
    auto fullName = fmt::format("{}:{}", prefix, qname.localName());
    return this->intern(fullName);
  }

  template <bool AssureInterned = false>
  InternRef intern(StrRef str) {
    if (str.empty())
      return {nullptr, 0};
    if constexpr (AssureInterned) {
      auto* raw = const_cast<Char*>(str.data());
      return {raw, str.size()};
    }
    return this->makePooledStr(str);
  }

  InternRef makePooledStr(StrRef str) {
    if EXICPP_UNLIKELY(str.empty())
      return {nullptr, 0};
    const std::size_t len = str.size();
    Char* rawStr = doc->allocate_string(nullptr, len);
    std::char_traits<Char>::copy(rawStr, str.data(), len);

    InternRef is {rawStr, len};
    return is;
  }

  template <bool AssureInterned = false>
  XMLNode* makeNode(Ty type, StrRef name = "", StrRef value = "") {
    auto iname = this->intern<AssureInterned>(name);
    auto ivalue = this->intern<AssureInterned>(value);
    return doc->allocate_node(type,
      iname.data(), ivalue.data(),
      iname.size(), ivalue.size()
    );
  }

  template <bool AssureInterned = false>
  XMLAttribute* makeAttribute(StrRef name, StrRef value = "") {
    auto iname = this->intern<AssureInterned>(name);
    auto ivalue = this->intern<AssureInterned>(value);
    return doc->allocate_attribute(
      iname.data(), ivalue.data(),
      iname.size(), ivalue.size()
    );
  }

private:
  Box<XMLDocument> doc;
  XMLNode* node = nullptr;
  XMLAttribute* attr = nullptr;
};

} // namespace `anonymous`

Box<XMLDocument> exi_to_xml(const fs::path& path) noexcept {
  auto path_str = [&path]() -> Str {
    return to_multibyte(path);
  };

  HeapBuffer bufBase {(2048 * 32) - 1};
  BinaryBuffer buf {bufBase};
  if (Error E = buf.readFile(path)) {
    LOG_ERROR("Error opening '{}': {}", path_str(), E.message());
    return nullptr;
  }

  try {
    XMLBuilder builder {};
    auto parser = Parser::New(builder, buf);
    if (Error E = parser.parseHeader()) {
      LOG_ERROR("Error parsing header in '{}': {}", path_str(), E.message());
      return nullptr;
    }

    if (Error E = parser.parseAll()) {
      LOG_ERROR("Error in '{}': {}", path_str(), E.message());
      return nullptr;
    }

    return builder.take();
  } catch (const std::exception& e) {
    LOG_ERROR("Exception in '{}': {}", path_str(), e.what());
    return nullptr;
  }
}
