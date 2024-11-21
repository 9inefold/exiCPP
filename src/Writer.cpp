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
# define PRINT_NEWLINE() \
  do { if (DEBUG_GET_MODE()) fmt::println(""); } while(0)
# define INTERNAL_LOG(err) do { \
    if (DEBUG_GET_MODE()) { \
      fmt::println(""); \
      LOG_ERRCODE(err); \
    }} while(0)
#else
# define PRINT_NEWLINE() (void(0))
# define INTERNAL_LOG(err) (void(0))
#endif

#define HANDLE_FN(fn, ...)                                \
do { const auto err_code = (serialize.fn(__VA_ARGS__));   \
if EXICPP_UNLIKELY(err_code != exip::EXIP_OK) {           \
  serialize.closeEXIStream(&stream);                      \
  INTERNAL_LOG(err_code);                                 \
  return Error::From(ErrCode(err_code));                  \
}} while(0)

//======================================================================//
// Utils
//======================================================================//

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

//======================================================================//
// Namespaces
//======================================================================//

namespace {

class NsStack {
public:
  using Type = XMLAttribute;
  using EntryType = Type*;
  using It  = const EntryType*;
  using EntriesType = std::vector<EntryType>;

  struct Level {
    std::uint64_t start = 0; // entry starting offset
    std::uint32_t count = 0; // element count
    std::uint32_t depth = 1; // number of nested levels
    EntryType inline_ns = nullptr;
  };

  struct EntrySpan {
    It begin() const { return I; } 
    It end() const { return E; } 
  public:
    It I = nullptr;
    It E = nullptr;
  };

public:
  void incDepth(bool isNewLevel);
  void decDepth();
  void addEntry(EntryType entry);
  void addEntries(const EntriesType& entries);
  EntryType findEntry(StrRef prefix) const;
  EntryType findInlineEntry() const;
  StrRef findEntryUri(StrRef prefix) const;
private:
  void addInlineNs(EntryType entry);
  EntrySpan getEntries(const Level& level) const;
  void splitTopLevel();
  Level& curr();
  const Level& curr() const;
  bool shouldSplit() const;
  bool isCurrentLevelEmpty() const;
private:
  std::vector<Level> level_buf;
  EntriesType entry_buf;
};

void NsStack::incDepth(bool isNewLevel) {
  if (isNewLevel) {
    // LOG_ASSERT(!level_buf.empty());
    level_buf.push_back({level_buf.size()});
    return;
  }
  if EXICPP_UNLIKELY(level_buf.empty()) {
    level_buf.push_back({0});
    return;
  }
  ++level_buf.back().depth;
}

void NsStack::decDepth() {
  if (level_buf.empty()) {
    // LOG_WARN("NsStack::level_buf is empty.");
    return;
  }
  Level& last = level_buf.back();
  // Check if exiting scope.
  if (--last.depth == 0) {
    const auto count = std::size_t(last.count);
    entry_buf.resize(entry_buf.size() - count);
    level_buf.pop_back();
  }
}

void NsStack::addEntry(NsStack::EntryType entry) {
  LOG_ASSERT(entry != nullptr);
  LOG_ASSERT(!level_buf.empty());
  if (this->shouldSplit()) {
    this->splitTopLevel();
  }
  if (getName(entry) == "xmlns") {
    /// This is an inline namespace.
    LOG_ASSERT(entry->value_size() > 0);
    curr().inline_ns = entry;
    return;
  }
  entry_buf.push_back(entry);
  ++curr().count;
}

void NsStack::addEntries(const NsStack::EntriesType& entries) {
  if (entries.empty())
    return;
  LOG_ASSERT(!level_buf.empty());
  if (this->shouldSplit()) {
    this->splitTopLevel();
  }

  std::size_t len = entries.size();
  entry_buf.reserve(entry_buf.size() + len);
  for (auto* entry : entries) {
    if (getName(entry) == "xmlns") {
      this->addInlineNs(entry);
      --len;
      continue;
    }
    entry_buf.push_back(entry);
  }
  curr().count += len;
}

NsStack::EntryType NsStack::findEntry(StrRef prefix) const {
  if (prefix.empty())
    return this->findInlineEntry();
  // Not inline
  const auto str = fmt::format("xmlns:{}", prefix);
  for (std::size_t Ix = level_buf.size(); Ix > 0; --Ix) {
    auto& level = level_buf[Ix - 1];
    const auto entries = this->getEntries(level);
    for (Type* entry : entries) {
      if (getName(entry) == str)
        return entry;
    }
  }

  return nullptr;
}

NsStack::EntryType NsStack::findInlineEntry() const {
  for (std::size_t Ix = level_buf.size(); Ix > 0; --Ix) {
    auto& level = level_buf[Ix - 1];
    if (level.inline_ns)
      return level.inline_ns;
  }
  return nullptr;
}

StrRef NsStack::findEntryUri(StrRef prefix) const {
  if (EntryType entry = this->findEntry(prefix))
    return getValue(entry);
  return "";
}

//////////////////////////////////////////////////////////////////////////

void NsStack::addInlineNs(EntryType entry) {
  // Can only have one inline namespace per level.
  LOG_ASSERT(curr().inline_ns == nullptr);
  curr().inline_ns = entry;
}

NsStack::EntrySpan NsStack::getEntries(
 const NsStack::Level& level) const {
  const auto count = level.count;
  if (count == 0)
    return {};
  Type* const* I = entry_buf.data() + level.start;
  return {I, I + count};
}

void NsStack::splitTopLevel() {
  LOG_ASSERT(!level_buf.empty());
  this->decDepth();
  this->incDepth(true);
}

NsStack::Level& NsStack::curr() {
  const auto& c = const_cast<const NsStack*>(this)->curr();
  return const_cast<Level&>(c);
}

const NsStack::Level& NsStack::curr() const {
  LOG_ASSERT(!level_buf.empty());
  return level_buf.back();
}

bool NsStack::shouldSplit() const {
  if (this->isCurrentLevelEmpty())
    return true;
  return curr().depth > 1;
}

bool NsStack::isCurrentLevelEmpty() const {
  if (level_buf.empty())
    return true;
  auto& c = curr();
  return (c.count == 0) && !c.inline_ns;
}

} // namespace `anonymous`

//======================================================================//
// Writer
//======================================================================//

namespace {

using InlNsStackType = std::vector<CString>;
using NsMappingType  = std::unordered_map<StrRef, StrRef>;

struct WriterImpl {
  static constexpr CString EMPTY_STR { nullptr, 0 };
public:
  WriterImpl() = default;
  Error init(
    XMLDocument* doc,
    const IBinaryBuffer& buf,
    Option<Options> opts,
    bool cookie);
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
  void incDepth() {
    ++this->depth;
    namespaces.incDepth(false);
  }
  void decDepth() {
    --this->depth;
    namespaces.decDepth();
  }
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
  NsStack namespaces;
};

Error WriterImpl::init(
  XMLDocument* doc,
  const IBinaryBuffer& buf,
  Option<Options> opts,
  bool cookie)
{
  this->doc  = doc;
  this->node = doc;

  serialize.initHeader(&stream);
  auto& header = stream.header;
  header.has_cookie = exip::boolean(cookie);
  header.has_options = exip::TRUE;
  
  if (opts.has_value()) {
    header.opts = opts->getBase();
    header.opts.valueMaxLength = INDEX_MAX;
    header.opts.valuePartitionCapacity = INDEX_MAX;
  }

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
      LOG_INFO("    DATA {}", value);
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

static StrRef GetNodeTypeName(XMLType ty) {
  switch (ty) {
   case XMLType::node_document:
    return "document";
   case XMLType::node_element:
    return "element";
   case XMLType::node_data:
    return "data";
   case XMLType::node_cdata:
    return "cdata";
   case XMLType::node_comment:
    return "comment";
   case XMLType::node_declaration:
    return "declaration";
   case XMLType::node_doctype:
    return "doctype";
   case XMLType::node_pi:
    return "pi";
   default:
    return "unknown";
  }
}

void WriterImpl::begElem(XMLNode* node) {
  if (node->type() == XMLType::node_data)
    return;
  CQName name = this->makeQName(node, true);
#if EXICPP_DEBUG
  if (this->hasName()) {
    const auto* ln = name.localName;
    const auto* uri = name.uri;
    LOG_INFO("SETYPE {}", GetNodeTypeName(node->type()));
    if (last_prefix.empty()) {
      LOG_INFO("SE {}", StrRef(ln->str, ln->length));
    } else {
      LOG_INFO("SE {} : {}",
        this->last_prefix, StrRef(ln->str, ln->length));
    }
    if (uri)
      LOG_INFO("  URI [{}]", StrRef(uri->str, uri->length));
  }
#endif
  serialize.startElement(&this->stream, name, &this->valueType);
}

void WriterImpl::endElem() {
  if (this->node->type() == XMLType::node_data)
    return;
#if EXICPP_DEBUG
  if (this->hasName())
    LOG_INFO("EE {}", getName(this->node));
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
        // LOG_FATAL("Inline namespaces are unimplemented!");
        nsDecls.push_back(attr);
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

  // std::sort(nsDecls.begin(), nsDecls.end(), sorter);
  namespaces.addEntries(nsDecls);
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
  const auto rawPrefix = (name.size() == 5) ? "" : name.substr(6);
  auto prefix = this->makeData(rawPrefix);
  auto uri = this->makeData(getValue(ns));
  
  if (rawPrefix.empty()) {
    LOG_INFO(" NSINL \"{}\"", uri);
    serialize.namespaceDeclaration(
      &this->stream, uri, prefix, exip::TRUE);
    return;
  }

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
    if (isElem) {
      this->last_prefix = "";
      // Find inl attr (if exists.)
      XMLAttribute* attr =
        this->node->first_attribute("xmlns", 5);
      if (attr) {
        this->uri = this->makeData(getValue(attr));
        ns = &this->uri;
      } else if (auto* lkup = namespaces.findInlineEntry()) {
        this->uri = this->makeData(getValue(lkup));
        ns = &this->uri;
      }
    }
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

  if (isElem) {
    this->last_prefix = prefix;
    // Find attr.
    auto attrName = fmt::format("xmlns:{}", prefix);
    XMLAttribute* attr = this->node->first_attribute(
      attrName.c_str(), attrName.size()
    );

    if (attr) {
      this->uri = this->makeData(getValue(attr));
      ns = &this->uri;
    } else if (auto* lkup = namespaces.findEntry(prefix)) {
      this->uri = this->makeData(getValue(lkup));
      ns = &this->uri;
    }
  }

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

Error exi::write_xml(
  XMLDocument* doc,
  const IBinaryBuffer& buf,
  Option<Options> opts,
  bool cookie)
{
  WriterImpl writer {};
  // TODO: Add opts
  if (Error E = writer.init(doc, buf, opts, cookie)) {
    return E;
  }
  return writer.parse();
}
