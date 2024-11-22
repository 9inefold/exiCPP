//===- utils/CompareXml.cpp -----------------------------------------===//
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

#include "CompareXml.hpp"
#include "Print.hpp"
#include "STL.hpp"
#include <exicpp/Debug/Format.hpp>
#include <fmt/format.h>

using namespace exi;

static StrRef getName(const XMLBase* node) {
  if EXICPP_UNLIKELY(!node) return "";
  if (node->name_size() == 0) return "";
  return {node->name(), node->name_size()};
}
static StrRef getValue(const XMLBase* node) {
  if EXICPP_UNLIKELY(!node) return "";
  if (node->value_size() == 0) return "";
  return {node->value(), node->value_size()};
}

static Str ReplaceNonprintable(StrRef S) {
  std::size_t I = 0, window = 0;
  const std::size_t E = S.size();

  fmt::basic_memory_buffer<Char, 64> out;
  out.reserve(S.size());
  auto push = [&, S] (bool extra = false) {
    out.append(S.substr(I, window - (I + extra)));
    I = window;
  };

  while (window < E) {
    using UChar = std::make_unsigned_t<Char>;
    const auto C = UChar(S[window++]);
    if (C >= UChar(' '))
      continue;
    push(true);
    fmt::format_to(std::back_inserter(out), "&#{};", unsigned(C));
  }

  push();
  return Str{out.data(), out.size()};
}

namespace {

struct XMLNodeIt {
  XMLNodeIt(XMLNode* node) : node(node) {}
  XMLNode* operator->() { return this->node; }
  const XMLNode* operator->() const { return this->node; }
  std::uint64_t currDepth() const { return this->depth; }
public:
  bool next() {
    // This works only because we always begin at the document level.
    if (node->first_node()) {
      node = node->first_node();
      ++this->depth;
      return true;
    }

    auto* parent = node->parent();
    if (!parent)
      return false;

    if (node->next_sibling()) {
      node = node->next_sibling();
      return true;
    }

    node = parent;
    parent = node->parent();
    --this->depth;

    while (parent) {
      if (node->next_sibling()) {
        node = node->next_sibling();
        return true;
      }

      node = parent;
      parent = node->parent();
      --this->depth;
    }

    return false;
  }

  StrRef name()  const { return getName(this->node); }
  StrRef value() const { return getValue(this->node); }

  Str nameS()  const { return ReplaceNonprintable(this->name()); }
  Str valueS() const { return ReplaceNonprintable(this->value()); }

  StrRef typeName() const {
    if EXICPP_UNLIKELY(!node)
      return "unknown";
    switch (node->type()) {
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

private:
  XMLNode* node = nullptr;
  std::uint64_t depth = 0;
};

static bool nextNode(XMLNodeIt& node) {
  // This works only because we always begin at the document level.
  if (node->first_node()) {
    node = node->first_node();
    return true;
  }

  auto* parent = node->parent();
  if (!parent)
    return false;

  if (node->next_sibling()) {
    node = node->next_sibling();
    return true;
  }

  node = parent;
  parent = node->parent();

  while (parent) {
    if (node->next_sibling()) {
      node = node->next_sibling();
      return true;
    }

    node = parent;
    parent = node->parent();
  }

  return false;
}

struct XMLCompare {
  bool skipIgnoredData(XMLNodeIt& oldNode) const;
  bool compareAttributes(
    XMLNodeIt& oldNode, XMLNodeIt& newNode,
    const std::size_t nodeCount
  ) const;
  bool compare(XMLNodeIt& oldNode, XMLNodeIt& newNode) const;
public:
  bool preserveComments = false;
  bool preservePIs = false;
  bool preserveDTs = false;
  bool verbose = false;
};

bool XMLCompare::skipIgnoredData(XMLNodeIt& oldNode) const {
  while (oldNode.next()) {
    switch (oldNode->type()) {
    // Only skip if comments disabled
     case XMLType::node_comment:
      if (!preserveComments)
        continue;
      return true;
    // Only skip if processing instructions disabled
     case XMLType::node_pi:
      if (!preservePIs)
        continue;
      return true;
    // Only skip if DOCTYPEs disabled
     case XMLType::node_doctype:
      if (!preserveDTs)
        continue;
      return true;
    // Only skip if a single newline.
     case XMLType::node_data:
      if (oldNode.value() == "\n")
        continue;
      [[fallthrough]];
    // Otherwise, don't skip
     default:
      return true;
    }
  }

  return false;
}

bool XMLCompare::compareAttributes(
  XMLNodeIt& oldNode,
  XMLNodeIt& newNode,
  const std::size_t nodeCount) const
{
  auto* rawOldAttr = oldNode->first_attribute();
  auto* rawNewAttr = newNode->first_attribute();
  const auto depth = oldNode.currDepth();

  if (!rawOldAttr || !rawNewAttr) {
    if (rawOldAttr || rawNewAttr) {
      const bool is_left = !rawOldAttr;
      StrRef side = is_left ? "right" : "left";
      StrRef name = is_left ? getName(rawNewAttr) : getName(rawOldAttr);
      PRINT_ERR("[#{}:{}] Attributes do not match,"
        " {}-side is not empty ({}).", nodeCount, depth, side, name);
      return false;
    }
    return true;
  }

  const auto collectAttrs = [](XMLAttribute* attrs) {
    Map<StrRef, StrRef> map;
    while (attrs) {
      map[getName(attrs)] = getValue(attrs);
      attrs = attrs->next_attribute();
    }
    return map;
  };

  auto oldAttrs = collectAttrs(rawOldAttr);
  auto newAttrs = collectAttrs(rawNewAttr);
  bool result = true;

  for (auto [key, val] : oldAttrs) {
    if (newAttrs.count(key) < 1) {
      result = false;
      PRINT_ERR("[#{}:{}] Attribute {} not found in new attributes.", 
        nodeCount, depth, key);
      continue;
    }

    auto newVal = newAttrs.extract(key);
    if (val != newVal.mapped()) {
      result = false;
      PRINT_ERR("[#{}:{}] Attribute {} values do not match: {} != {}.", 
        nodeCount, depth, key, val, newVal.mapped());
    }
  }

  if (newAttrs.size() != 0) {
    for (auto [key, val] : oldAttrs) {
      result = false;
      PRINT_ERR("[#{}:{}] Attribute {} not found in old attributes.", 
        nodeCount, depth, key);
    }
  }
  
  return result;
}

bool XMLCompare::compare(XMLNodeIt& oldNode, XMLNodeIt& newNode) const {
  int errorCount = 0;
  std::size_t nodeCount = 0;

  while (newNode.next()) {
    if (errorCount > 10) {
      PRINT_INFO("Exiting early, error count too high.");
      return false;
    }

    ++nodeCount;
    if (!skipIgnoredData(oldNode)) {
      PRINT_ERR("[#{}] Old XML ended prematurely! (New XML at <{}> as {})",
        nodeCount, newNode.name(), newNode.typeName());
      return false;
    } else if (verbose) {
      PRINT_INFO("Comparing <{}> and <{}>",
        oldNode.name(), newNode.name());
    }

    const auto depth = oldNode.currDepth();
    if (depth != newNode.currDepth()) {
      PRINT_ERR("[#{}] Inconsistent depths: {} != {}",
        nodeCount,
        depth, newNode.currDepth());
      return false;
    }

    if (oldNode->type() != newNode->type()) {
      PRINT_ERR("[#{}:{}] Inconsistent types: {} != {}",
        nodeCount, depth,
        oldNode.typeName(), newNode.typeName());
      ++errorCount;
    }
    if (oldNode.name() != newNode.name()) {
      PRINT_ERR("[#{}:{}] Inconsistent names: {} != {}",
        nodeCount, depth,
        oldNode.name(), newNode.name());
      ++errorCount;
    }
    if (oldNode.value() != newNode.value()) {
      PRINT_ERR("[#{}:{}] Inconsistent values: '{}' != '{}'",
        nodeCount, depth,
        oldNode.valueS(), newNode.valueS());
      ++errorCount;
    }

    if (!compareAttributes(oldNode, newNode, nodeCount))
      ++errorCount;
  }

  if (!skipIgnoredData(oldNode)) {
    return errorCount == 0;
  }

  if (oldNode->type() != XMLType::node_document) {
    PRINT_ERR("New XML ended prematurely! (Old XML at <{}> as {})",
      oldNode.name(), oldNode.typeName());
    return false;
  }

  if (const auto depth = oldNode.currDepth(); depth != 0) {
    PRINT_ERR("Old XML ended with a depth of {}", depth);
  }
  if (const auto depth = newNode.currDepth(); depth != 0) {
    PRINT_ERR("New XML ended with a depth of {}", depth);
  }

  return errorCount == 0;
}

} // namespace `anonymous`

bool compare_xml(XMLDocument* oldDoc, XMLDocument* newDoc, CompareOpts opts) {
  LOG_ASSERT(oldDoc != nullptr);
  if (!newDoc) {
    PRINT_ERR("Right-side XML document could not be parsed!");
    return false;
  }

  XMLNodeIt oldNode {oldDoc};
  XMLNodeIt newNode {newDoc};

  XMLCompare C {
    opts.preserveComments,
    opts.preservePIs,
    opts.preserveDTs,
    opts.verbose
  };
  return C.compare(oldNode, newNode);
}
