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

#include <exicpp/Writer.hpp>
#include <exip/stringManipulate.h>

using namespace exi;
using ::exip::serialize;
using Traits = std::char_traits<char>;

#define HANDLE_FN(fn, ...)                                \
do { const auto err_code = (serialize.fn(__VA_ARGS__));   \
if (err_code != exip::EXIP_OK) {                          \
  serialize.closeEXIStream(&stream);                      \
  return Error::From(ErrCode(err_code));                  \
}} while(0)

static const CString EMPTY_STR { nullptr, 0 };

static StrRef getName(XMLBase* data) {
  if (!data || data->name_size() == 0) return "";
  return {data->name(), data->name_size()};
}

static StrRef getValue(XMLBase* data) {
  if (!data || data->value_size() == 0) return "";
  return {data->value(), data->value_size()};
}

namespace {
struct WriterImpl {
  Error init(XMLDocument* doc, const StackBuffer& buf);
  Error parse();
private:
  void begElem(XMLNode* node);
  void endElem();
  void attrs(XMLNode* node);
  void attr(XMLAttribute* attrib);
private:
  CQName makeName(XMLNode* node);
  CString makeData(StrRef data, bool clone = false);
  bool nextNode();
private:
  exip::EXIStream stream;
  exip::EXITypeClass valueType;
  CString uri;
  CString localName;
  // ...
  XMLDocument* doc = nullptr;
  XMLNode* node = nullptr;
  XMLNode* last_node = nullptr;
};

Error WriterImpl::init(XMLDocument* doc, const StackBuffer& buf) {
  this->doc  = doc;
  this->node = doc;

  auto& header = stream.header;
  serialize.initHeader(&stream);

  header.has_cookie = exip::TRUE;
  header.has_options = exip::TRUE;
  header.opts.valueMaxLength = 300;
  header.opts.valuePartitionCapacity = 50;
  SET_STRICT(header.opts.enumOpt);

  HANDLE_FN(initStream,
    &stream, 
    (const exip::BinaryBuffer&)(buf),
    nullptr
  );

  HANDLE_FN(exiHeader, &stream);
  return Error::Ok();
}

Error WriterImpl::parse() {
  HANDLE_FN(startDocument, &stream);

  while (this->nextNode()) {
    this->attrs(node);
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

void WriterImpl::begElem(XMLNode* node) {
  const CQName name = this->makeName(node);
  serialize.startElement(&this->stream, name, &this->valueType);
}

void WriterImpl::endElem() {
  serialize.endElement(&this->stream);
}

void WriterImpl::attrs(XMLNode* node) {
  for (auto* attrib = node->first_attribute();
    attrib; attrib = attrib->next_attribute())
  {
    this->attr(attrib);
  }
}

void WriterImpl::attr(XMLAttribute* attrib) {
  const CQName name = this->makeName(node);
  serialize.attribute(&this->stream, name, exip::TRUE, &this->valueType);

  auto value = getValue(node);
  auto str = this->makeData(value);
  serialize.stringData(&this->stream, str);
}

//////////////////////////////////////////////////////////////////////////

CQName WriterImpl::makeName(XMLNode* node) {
  StrRef rawName = ::getName(node);
  const auto pos = rawName.find(':');
  if (pos == StrRef::npos) {
    CQName name {
      &EMPTY_STR,
      &this->localName,
      nullptr
    };
    
    auto* data = const_cast<Char*>(rawName.data());
    this->localName = CString{data, rawName.size()};
    return name;
  }

  EXICPP_UNREACHABLE();
  return {};
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

bool WriterImpl::nextNode() {
  last_node = node;
  if (node->first_node()) {
    // Begin the parent element.
    this->begElem(node);
    node = node->first_node();
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
    node = parent;
    parent = node->parent();
  }

  return false;
}

} // namespace `anonymous`

static void getQName(CQName& name, XMLNode* node) {
  StrRef wholeName = getName(node);
  const auto loc = wholeName.find(':');
  if (loc == StrRef::npos) {
    name = {&EMPTY_STR, &EMPTY_STR};
    return;
  }
  
  auto prefix = wholeName.substr(0, loc);
  auto postfix = wholeName.substr(loc);
  assert(0 && "Unimplemented");
}

namespace exi {
Error write_xml(XMLDocument* doc, const StackBuffer& buf) {
  WriterImpl writer {};
  if (Error E = writer.init(doc, buf)) {
    return E;
  }
  return writer.parse();
}
} // namespace exi;
