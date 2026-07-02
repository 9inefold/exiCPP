//===- exi/Decode/XMLDeserializer.cpp --------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
///
/// \file
/// This file implements the interface used to decode EXI as XML.
///
//===----------------------------------------------------------------===//

#include <exi/Decode/XMLDeserializer.hpp>
#include <exi/Decode/BodyDecoder.hpp>

#define DEBUG_TYPE "XMLDeserializer"

using namespace exi;

namespace {
static constexpr StrRef kInitialURIPartitionPrefix[] {"", "xml", "xsi", "xsd"};
} // namespace `anonymous`

StrRef XMLDeserializer::getUnboundPrefixUniversal(StrRef URI, StrRef LocalName) {
  // Creates a `{URI}LocalName` name.
  ScratchBuf.clear();
  raw_svector_ostream OS(ScratchBuf);
  OS << '{' << URI << '}' << LocalName;
  return intern(ScratchBuf.str());
}

StrRef XMLDeserializer::getUnboundPrefixExificient(StrRef URI, StrRef LocalName, u64 ID) {
  bool DidEmplace = getNSSlot(ID).second;
  ScratchBuf.clear();
  // Creates a `nsURI:LocalName` name.
  exi::format("ns{}", ID).toVector(ScratchBuf);
  return getUnboundPrefixCommon(URI, LocalName, DidEmplace);
}

/// Creates a name like `p0:LocalName`.
StrRef XMLDeserializer::getUnboundPrefixOpenexi(StrRef URI, StrRef LocalName, u64 ID) {
  auto [NewID, DidEmplace] = getNSSlot(ID);
  ScratchBuf.clear();
  // Creates a `pID:LocalName` name.
  exi::format("p{}", NewID).toVector(ScratchBuf);
  return getUnboundPrefixCommon(URI, LocalName, DidEmplace);
}

StrRef XMLDeserializer::getUnboundPrefixCommon(StrRef URI, StrRef LocalName, bool IsNew) {
  StrRef Prefix = ScratchBuf.str();
  if (IsNew) {
    const auto Name = QName::New(URI, Prefix, "xmlns"_str);
    auto* LocalAttr = allocAttr</*IsNS=*/true>(Name, intern(URI));
    Curr->prepend_attribute(LocalAttr);
    //LOG_EXTRA("Added new unbound prefix: {}=\"{}\"",
    //          LocalAttr->name(), LocalAttr->value());
  }
  return intern(Prefix, LocalName);
}

static Option<unsigned> GetInitialURIPartitionPrefix(StrRef URI) {
  // Check sizes first?
  if (URI.empty())
    return 0;
  if (!URI.consume_front("http://www.w3.org/"))
    return std::nullopt;
  if (URI.consume_front("2001/XMLSchema")) {
    if (URI.empty())
      return 3;
    else if (URI == "-instance")
      return 2;
    else
      return std::nullopt;
  }
  if (URI == "XML/1998/namespace")
    return 1;
  else
    return std::nullopt;
}

static XMLNode* FindFirstSE(XMLDocument* Doc) {
  unsigned ItersLeft = 30;
  XMLNode* Curr = Doc->first_node();
  while (Curr && ItersLeft) {
    if (Curr->type() == xml::NodeKind::node_element)
      return Curr;
    Curr = Curr->next_sibling();
    --ItersLeft;
  }
  return nullptr;
}

Option<StrRef> XMLDeserializer::getInitialURIPartitionPrefix(StrRef URI) {
  Option ID = GetInitialURIPartitionPrefix(URI);
  if (!ID.has_value())
    return None;
  // Create the entry.
  exi_guard_invariant(*ID <= 3);
  const unsigned EntryID = *ID;
  StrRef Entry = kInitialURIPartitionPrefix[EntryID];
  if (!ActiveInitialURIPartitions.test(EntryID) && Curr) {
    XMLNode* TopLevel = FindFirstSE(Doc->document());
    const auto Name = QName::New(URI, Entry, "xmlns"_str);
    auto* LocalAttr = allocAttr</*IsNS=*/true>(Name, intern(URI));
    if (!TopLevel)
      Curr->prepend_attribute(LocalAttr);
    else {
      TopLevel->prepend_attribute(LocalAttr);
      ActiveInitialURIPartitions.set(EntryID);
    }
  }
  return Some(Entry);
}

StrRef XMLDeserializer::getURIPrefixForUnbound(StrRef URI, StrRef LocalName, u64 ID) {
  exi_invariant(!URI.empty());
  if (Option Entry = this->getInitialURIPartitionPrefix(URI))
    return intern(*Entry, LocalName);
  // Create new name.
  switch (XMLCoderOptions::UURIType) {
  case UURI_UNIVERSAL:
    return getUnboundPrefixUniversal(URI, LocalName);
  case UURI_EXIFICIENT:
    return getUnboundPrefixExificient(URI, LocalName, ID);
  case UURI_OPENEXI:
    return getUnboundPrefixOpenexi(URI, LocalName, ID);
  case UURI_CUSTOM:
    exi_todo("UURI_CUSTOM not implemented");
  }
}

std::pair<u64, bool> XMLDeserializer::getNSSlot(const u64 ID) {
  exi_invariant(NSSlots);
  const u64 Slot = NSSlots->size();
  auto [It, DidEmplace] = NSSlots->try_emplace(ID, Slot);
  return {It->second, DidEmplace};
}

void XMLDeserializer::setURIPrefixForUnboundSE() {
  if (!hasUnboundPrefix())
    return;
  if (CurrURI.empty())
    return;
  // TODO: Handle xml/xsi namespaces? 
  // Create new name.
  StrRef LocalName = Curr->name();
  StrRef FullName = getURIPrefixForUnbound(CurrURI, LocalName, UnboundURI);
  Curr->name(FullName);
  this->CurrURI = "";
}

StrRef XMLDeserializer::getFullNameAT(const QName& Name, bool IsNS) {
  StrRef LocalName = Name.name();
  if (IsNS) {
    if (LocalName.empty())
      return "xmlns"_str;
    return intern("xmlns"_str, LocalName);
  } else if (Name.hasPrefix()) {
    auto Pfx = Name.pfx();
    if (LocalName.empty()) {
      LOG_WARN("Empty name AT: {}:=\"{}\"", Pfx, Name.uri());
      return intern(Pfx, LocalName);
    }
    if (!Pfx.empty())
      return intern(Name.pfx(), LocalName);
  } else if (StrRef URI = Name.uri(); !URI.empty()) {
    return getURIPrefixForUnbound(URI, LocalName, Name.id());
  }
  return intern(LocalName);
}
