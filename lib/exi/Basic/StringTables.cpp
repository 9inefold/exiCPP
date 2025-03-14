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
#include <exi/Basic/ExiOptions.hpp>
#include <algorithm>

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

template <typename T>
static T GetBoundsOrDefault(Bounded<T> I) {
  if (I == unbounded)
    return static_cast<T>(kDefaultReserveSize);
  return I;
}

static const Option<u64> PullSchemaID(Option<Option<u64>> SchemaID) {
  return SchemaID.expect("schema should resolve to value or nil");
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

  auto ID = PullSchemaID(Opts.SchemaID);
  const bool UsesSchema = ID.has_value();

  /// Populates the URI, Prefix, and LocalName partitions.
  createInitialEntries(UsesSchema);
  if (UsesSchema) {
    // TODO: Reserve for schema.
    // SchemaResolver[*ID]->getExtraEntryCount();
  }

  const u64 ValueCapacity
    = GetBoundsOrDefault(Opts.ValuePartitionCapacity);
  GValueMap.reserve(ValueCapacity);

  if (Opts.DatatypeRepresentationMap) {
    // TODO: DatatypeRepresentationMap?
    exi_unreachable("datatype mapping is unsupported.");
  }
}

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  // Saving these is ok since we know there are at least 4 inline slots in
  // the partition.
  auto Empty = createURI("", "").second;
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
    InlineStr* PfxP = intern(Pfx);
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
