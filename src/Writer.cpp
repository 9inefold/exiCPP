//===- Writer.cpp ---------------------------------------------------===//
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

#include <Writer.hpp>
#include <Debug/Format.hpp>
#include <exip/stringManipulate.h>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <fmt/format.h>
#include <fmt/color.h>

using namespace exi;
namespace fs = std::filesystem;
using ::exip::serialize;
using Traits = std::char_traits<char>;
using VPtr = void*;

#if EXICPP_DEBUG && (EXICPP_DEBUG_LEVEL <= ERROR)
# define PRINT_NEWLINE() fmt::println("")
#else
# define PRINT_NEWLINE() (void(0))
#endif

#define HANDLE_FN(fn, ...)                                \
do { const auto err_code = (serialize.fn(__VA_ARGS__));   \
if (err_code != exip::EXIP_OK) {                          \
  serialize.closeEXIStream(&stream);                      \
  PRINT_NEWLINE();                                        \
  LOG_ERRCODE(err_code);                                  \
  return Error::From(ErrCode(err_code));                  \
}} while(0)

//////////////////////////////////////////////////////////////////////////

static StrRef getName(XMLBase* data) {
  if EXICPP_UNLIKELY(!data) return "";
  if (data->name_size() == 0) return "";
  return {data->name(), data->name_size()};
}

static StrRef getValue(XMLBase* data) {
  if EXICPP_UNLIKELY(!data) return "";
  if (data->value_size() == 0) return "";
  return {data->value(), data->value_size()};
}

namespace {

using InlNsStackType = std::vector<CString>;
using NsMappingType  = std::unordered_map<StrRef, StrRef>;

struct WriterImpl {
  static constexpr CString EMPTY_STR { nullptr, 0 };
public:
  WriterImpl() { ns_stack.reserve(4); }
  Error init(XMLDocument* doc, const StackBuffer& buf);
  Error parse();
private:
  void begElem(XMLNode* node);
  void endElem();
  void handleAttrs(XMLNode* node);
  void handleNs(XMLAttribute* attrib);
  void handleAttr(XMLAttribute* attrib);
private:
  CQName makeQName(XMLBase* node, bool isElem = false);
  CString makeData(StrRef data, bool clone = false);
private:
  bool nextNode();
  void incDepth() { ++this->depth; }
  void decDepth() { --this->depth; }
  bool hasName() const;
  bool hasValue() const;
private:
  exip::EXIStream stream {};
  exip::EXITypeClass valueType;
  CString uri {};
  CString localName {};
  CString prefix {};
  // ...
  XMLDocument* doc = nullptr;
  XMLNode* node = nullptr;
  XMLNode* last_node = nullptr;
  StrRef last_prefix = "";
  std::int64_t depth = 0;
  InlNsStackType ns_stack;
  NsMappingType  ns_map;
};

Error WriterImpl::init(XMLDocument* doc, const StackBuffer& buf) {
  this->doc  = doc;
  this->node = doc;
  serialize.initHeader(&stream);

  auto& header = stream.header;
  header.has_cookie = exip::TRUE;
  header.has_options = exip::TRUE;
  header.opts.valueMaxLength = INDEX_MAX;
  header.opts.valuePartitionCapacity = INDEX_MAX;
  // SET_STRICT(header.opts.enumOpt);
  SET_PRESERVED(header.opts.preserve, PRESERVE_PREFIXES);

  HANDLE_FN(initStream,
    &stream, 
    (const exip::BinaryBuffer&)(buf),
    nullptr
  );

  HANDLE_FN(exiHeader, &stream);
  return Error::Ok();
}

Error WriterImpl::parse() {
  LOG_ASSERT(this->node != nullptr);
  HANDLE_FN(startDocument, &stream);

  if (node->type() != XMLType::node_document) {
    serialize.closeEXIStream(&stream);
    return Error::From("Top level node was not a document!");
  }

  while (this->nextNode()) {
    this->handleAttrs(node);
    if (node->type() == XMLType::node_data) {
      auto value = getValue(node);
      auto str = this->makeData(value);
      serialize.stringData(&this->stream, str);
    }
  }

  HANDLE_FN(endDocument, &stream);
  HANDLE_FN(closeEXIStream, &stream);
  return Error::Ok();
}

//////////////////////////////////////////////////////////////////////////

static bool StartsWith(StrRef S, StrRef toCmp) {
  const auto len = std::min(S.size(), toCmp.size());
  return S.substr(0, len) == toCmp;
}

static bool ConsumeStartsWith(StrRef& S, StrRef toCmp) {
  const bool ret = StartsWith(S, toCmp);
  if (!ret)
    return false;
  S = S.substr(toCmp.length());
  return true;
}

void WriterImpl::begElem(XMLNode* node) {
  CQName name = this->makeQName(node, true);
  if (name.prefix) {
    const auto* pre = name.prefix;
    auto attrName = std::string("xmlns:")
      + std::string(pre->str, pre->length);
    auto* attr = node->first_attribute(
      attrName.c_str(), attrName.size());
    if (attr) {
      this->uri = this->makeData(getValue(attr));
      name.uri = &this->uri;
    }
  }

#if EXICPP_DEBUG
  if (this->hasName())
    LOG_INFO("<{}>: {}", getName(node), VPtr(node));
#endif
  serialize.startElement(&this->stream, name, &this->valueType);
}

void WriterImpl::endElem() {
#if EXICPP_DEBUG
  if (this->hasName())
    LOG_INFO("</{}>: {}", getName(this->node), VPtr(this->node));
#endif
  serialize.endElement(&this->stream);
}

void WriterImpl::handleAttrs(XMLNode* node) {
  using AttrCacheType = std::vector<XMLAttribute*>;
  AttrCacheType nsDecls;
  AttrCacheType attrs;

  for (auto* attr = node->first_attribute();
    attr; attr = attr->next_attribute())
  {
    using namespace std::literals;
    auto name = getName(attr);
    LOG_ASSERT(!name.empty());

    if (ConsumeStartsWith(name, "xmlns"sv)) {
      if (name.empty()) {
        // TODO: Use nonLocalNs?
        LOG_FATAL("Inline namespaces are unimplemented!");
        continue;
      } else if (name.front() == ':') {
        nsDecls.push_back(attr);
        continue;
      }
    }
    // TODO: Deal with xsi:*
    attrs.push_back(attr);
  }

  const auto sorter = [](XMLAttribute* a, XMLAttribute* b) {
    return getName(a) < getName(b);
  };

  std::sort(nsDecls.begin(), nsDecls.end(), sorter);
  std::sort(attrs.begin(), attrs.end(), sorter);

  for (auto* ns : nsDecls)
    this->handleNs(ns);
  for (auto* attr : attrs)
    this->handleAttr(attr);
}

void WriterImpl::handleNs(XMLAttribute* ns) {
  const auto name = getName(ns);
  LOG_ASSERT(StartsWith(name, "xmlns"));
  // sizeof("xmlns:") - 1
  const auto rawPrefix = name.substr(6);
  auto prefix = this->makeData(rawPrefix);
  auto uri = this->makeData(getValue(ns));
  LOG_INFO(" NS {}=\"{}\"", prefix, uri);
  const auto localNs = exip::boolean(rawPrefix == this->last_prefix);
  serialize.namespaceDeclaration(&this->stream, uri, prefix, localNs);
}

void WriterImpl::handleAttr(XMLAttribute* attr) {
  LOG_INFO(" AT {}=\"{}\"", getName(attr), getValue(attr));
  const CQName name = this->makeQName(attr);
  serialize.attribute(&this->stream, name, exip::FALSE, &this->valueType);

  auto str = this->makeData(::getValue(attr));
  serialize.stringData(&this->stream, str);
}

//////////////////////////////////////////////////////////////////////////

static CString MakeString(StrRef str) {
  if (str.empty())
    return {nullptr, 0};
  auto* data = const_cast<Char*>(str.data());
  return {data, str.size()};
}

CQName WriterImpl::makeQName(XMLBase* node, bool isElem) {
  const auto* ns = &EMPTY_STR;
  StrRef rawName = ::getName(node);
  const auto pos = rawName.find(':');
  if (pos == StrRef::npos) {
    if (isElem)
      this->last_prefix = "";
    this->localName = MakeString(rawName);
    return {
      ns,
      &this->localName,
      nullptr
    };
  }

  auto prefix = rawName.substr(0, pos);
  auto postfix = rawName.substr(pos + 1);
  this->prefix = MakeString(prefix);
  this->localName = MakeString(postfix);
  // this->localName = MakeString(rawName);

#if 1
  if (isElem) {
    this->last_prefix = prefix;
    // Find attr.
    XMLAttribute* attr = nullptr;
    if (!(attr = this->node->first_attribute("xmlns", 5))) {
      auto attrName = std::string("xmlns:") + std::string(prefix);
      auto* attr = this->node->first_attribute(
        attrName.c_str(), attrName.size());
    }
    if (attr) {
      this->uri = this->makeData(getValue(attr));
      ns = &this->uri;
    }
  }
#endif

  return {
    ns,
    &this->localName,
    &this->prefix
  };
}

CString WriterImpl::makeData(StrRef data, bool clone) {
  CString str {};
  exip::asciiToStringN(
    data.data(), data.size(), 
    &str, &stream.memList,
    exip::boolean(clone)
  );
  return str;
}

//////////////////////////////////////////////////////////////////////////

bool WriterImpl::nextNode() {
  last_node = node;
  // This works only because we always begin at the document level.
  if (node->first_node()) {
    node = node->first_node();
    this->incDepth();
    // Begin the child element.
    this->begElem(node);
    return true;
  }

  auto* parent = node->parent();
  if (!parent) return false;

  if (node->next_sibling()) {
    // End the current element.
    this->endElem();
    node = node->next_sibling();
    // Begin the sibling element.
    this->begElem(node);
    return true;
  }

  // End the child element.
  this->endElem();
  node = parent;
  parent = node->parent();
  this->decDepth();

  while (parent) {
    if (node->next_sibling()) {
      // End the current parent element.
      this->endElem();
      node = node->next_sibling();
      // Begin the sibling element.
      this->begElem(node);
      return true;
    }

    // End the child element.
    this->endElem();
    last_node = node;
    node = parent;
    parent = node->parent();
    this->decDepth();
  }

  return false;
}

bool WriterImpl::hasName() const {
  const auto ty = node->type();
  return
    (ty == XMLType::node_element) || 
    (ty == XMLType::node_pi);
}

bool WriterImpl::hasValue() const {
  const auto ty = node->type();
  return
    (ty != XMLType::node_document) &&
    (ty != XMLType::node_declaration);
}

} // namespace `anonymous`

namespace exi {
Error write_xml(XMLDocument* doc, const StackBuffer& buf) {
  WriterImpl writer {};
  if (Error E = writer.init(doc, buf)) {
    return E;
  }
  return writer.parse();
}
} // namespace exi;
