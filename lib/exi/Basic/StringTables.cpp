//===- exi/Basic/StringTables.hpp -----------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines the various tables used by the EXI processor.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/StringTables.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <algorithm>

#define DEBUG_TYPE "StringTables"

using namespace exi;

namespace {
enum : u64 { kDefaultReserveSize = 64 };

constexpr StrRef XML_URI("http://www.w3.org/XML/1998/namespace");
constexpr StrRef XML_InitialValues[] { "base", "id", "lang", "space" };

constexpr StrRef XSI_URI("http://www.w3.org/2001/XMLSchema-instance");
constexpr StrRef XSI_InitialValues[] { "nil", "type" };

constexpr StrRef XSD_URI("http://www.w3.org/2001/XMLSchema");
constexpr StrRef XSD_InitialValues[] {
  "ENTITIES", "ENTITY", "ID", "IDREF", "IDREFS",
  "NCName", "NMTOKEN", "NMTOKENS", "NOTATION", "Name", "QName",
  "anySimpleType", "anyType", "anyURI",
  "base64Binary", "boolean", "byte",
  "date", "dateTime", "decimal", "double", "duration", "float",
  "gDay", "gMonth", "gMonthDay", "gYear", "gYearMonth",
  "hexBinary", "int", "integer", "language", "long",
  "negativeInteger", "nonNegativeInteger", "nonPositiveInteger",
  "normalizedString", "positiveInteger", "short", "string", "time", "token",
  "unsignedByte", "unsignedInt", "unsignedLong", "unsignedShort"
};

} // namespace `anonymous`

static const Option<String&> PullSchemaID(const Option<MaybeBox<String>>& ID) {
  return ID.expect("schema should resolve to value or nil").get();
}

//===----------------------------------------------------------------===//
// Decoding
//===----------------------------------------------------------------===//

namespace exi::decode {

StringTable::StringTable() : LNMap(LNPageAllocator) {
  GValueMap.reserve(kDefaultReserveSize);
}

void StringTable::setup(const ExiOptions& Opts) {
  if (DidSetup)
    return;
  DidSetup = true;

  Option<const String&> ID = PullSchemaID(Opts.SchemaID);
  const bool UsesSchema = ID.has_value();

  /// Populates the URI, Prefix, and LocalName partitions.
  createInitialEntries(UsesSchema);
  if (UsesSchema) {
    // TODO: Reserve for schema.
    // SchemaResolver[*ID]->getExtraEntryCount();
  }

  if (Bounded I = Opts.ValuePartitionCapacity; I.bounded()) {
    WrappingValues = true;
    LOG_WARN("Bounded tables are not supported, "
             "the value '{}' only affects the initial reserve.", *I);
    // TODO: Do some kind of check here. We definitely don't want arbitrarily
    // large allocations.
    GValueMap.reserve(*I);
  } else
    GValueMap.reserve(kDefaultReserveSize);

  if (Opts.DatatypeRepresentationMap) {
    // TODO: DatatypeRepresentationMap?
    exi_unreachable("datatype mapping is unsupported.");
  }
}

IDPair StringTable::addURI(StrRef URI, Option<StrRef> Pfx) {
  // const CompactID ID = *URICount;
  auto [Info, ID] = createURI(URI, Pfx);
  return {Info->Name, ID};
}

StrRef StringTable::addPrefix(CompactID URI, StrRef Pfx) {
  exi_invariant(URI < URIMap.size());
  this->assertPartitionsInSync();

  ++URIMap[URI].PrefixElts;
  InlineStr* PfxP = intern(Pfx);
  PrefixMap[URI].push_back(PfxP);

  return PfxP->str();
}

IDPair StringTable::addLocalName(CompactID URI, StrRef Name) {
  exi_invariant(URI < URIMap.size());
  this->assertPartitionsInSync();

  const CompactID ID = URIMap[URI].LNElts++;
  LocalName* LN = createLocalName(Name);
  LNMap[URI].push_back(LN);

  return {LN->Name, ID};
}

StrRef StringTable::addValue(StrRef Value) {
  // Add to the global table, no other interaction needed.
  return createGlobalValue(Value)->str();
}

StrRef StringTable::addValue(CompactID URI, CompactID LocalID, StrRef Value) {
  exi_invariant(URI < URIMap.size());
  exi_invariant(LocalID < *LNCount);
  this->assertPartitionsInSync();

  // Add to the global table.
  InlineStr* Str = createGlobalValue(Value);
  // Add to the local table for URI:LocalID.
  LNMap[URI][LocalID]->LocalValues.push_back(Str);
  return Str->str();
}

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  // Saving these is ok since we know there are at least 4 inline slots in
  // the partition.
  auto Empty = createURI(""_str, ""_str).second;
  auto Xml = createURI(XML_URI, "xml"_str).second;
  auto Xsi = createURI(XSI_URI, "xsi"_str).second;

  // D.3 - Initial Entries in LocalName Partitions
  appendLocalNames(Xml, XML_InitialValues);
  appendLocalNames(Xsi, XSI_InitialValues);

  if (UsesSchema) {
    // TODO: When a schema is provided, prepopulate with the LocalName of each
    // attribute, element and type explicitly declared in the schema.
    auto Xsd = createURI(XSD_URI).second;
    appendLocalNames(Xsd, XSD_InitialValues);
  }
}

std::pair<URIInfo*, CompactID>
 StringTable::createURI(StrRef URI, Option<StrRef> Pfx) {
  this->assertPartitionsInSync();

  // Get our result ID.
  const CompactID ID = *URICount++;  
  StrRef Interned = internStr(URI);
  const u32 PfxElts = Pfx ? 1 : 0;

  // Create a new URI entry.
  URIInfo* URIPart = &URIMap.emplace_back(Interned, PfxElts, 0u);

  // Create a Prefix partition entry even if no Prefix is provided. This keeps
  // our partitions in sync.
  auto& PfxPart = PrefixMap.emplace_back();
  if (Pfx) {
    InlineStr* PfxP = intern(*Pfx);
    PfxPart.push_back(PfxP);
  }

  // Create space for the new LocalName.
  LNMap.resize(*++LNCount);

  return {URIPart, ID};
}

void StringTable::appendLocalNames(CompactID ID, ArrayRef<StrRef> LocalNames) {
  exi_invariant(ID < *LNCount);
  LNMapType& NameMap = LNMap[ID];
  for (StrRef Local : LocalNames) {
    auto* LN = createLocalName(Local);
    NameMap.push_back(LN);
  }
}

} // namespace exi::decode

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

namespace exi::encode {

class StringTable;

} // namespace exi::encode
