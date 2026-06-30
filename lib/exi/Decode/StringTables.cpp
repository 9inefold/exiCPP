//===- exi/Decode/StringTables.cpp ----------------------------------===//
//
// Copyright (C) 2025 Ninefold
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

#include <exi/Decode/StringTable.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/D/ExiInitialValues.impl>
#include <algorithm>

#define DEBUG_TYPE "Decode.StringTables"
#define GEN_IV(URI, PFX, LV) #LV
#define SEP ,

using namespace exi;

namespace {
enum : u64 {
  kDefaultReserveSize = 64,
  kMaxInitialReserveSize = (4096 * 16),
};

constexpr StrRef XML_URI("http://www.w3.org/XML/1998/namespace");
constexpr StrRef XML_InitialValues[] { EXI_XML_IV(GEN_IV, SEP) };

constexpr StrRef XSI_URI("http://www.w3.org/2001/XMLSchema-instance");
constexpr StrRef XSI_InitialValues[] { EXI_XSI_IV(GEN_IV, SEP) };

constexpr StrRef XSD_URI("http://www.w3.org/2001/XMLSchema");
constexpr StrRef XSD_InitialValues[] { EXI_XSD_IV(GEN_IV, SEP) };

} // namespace `anonymous`

static const Option<String&> PullSchemaID(const Option<PackedMaybeBox<String>>& ID) {
  return ID.expect("schema should resolve to value or nil").get();
}

static constexpr u64 CapInitialReserve(usize I) {
  return (I < kMaxInitialReserveSize) ? I : kMaxInitialReserveSize;
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
    exi_todo("Reserve for schema.");
    // SchemaResolver[*ID]->getExtraEntryCount();
  }

  if (Bounded I = Opts.ValuePartitionCapacity; I.bounded()) {
    const usize InitialReserve = CapInitialReserve(*I);
    WrappingValues = true;
    LOG_WARN("Bounded tables are not supported, "
             "the value '{}' only affects the initial reserve.", InitialReserve);
    GValueMap.reserve(InitialReserve);
  } else
    GValueMap.reserve(kDefaultReserveSize);

  if (Opts.DatatypeRepresentationMap) {
    exi_unimplemented("DatatypeRepresentationMap is unsupported.");
  }
}

IDPair StringTable::addURI(StrRef URI, Option<StrRef> Pfx) {
  // const CompactID ID = *URICount;
  auto [Info, ID] = createURI(URI, Pfx);
  return {Info->Name, ID};
}

IDPair StringTable::addPrefix(CompactID URI, StrRef Pfx) {
  exi_invariant(URI < URIMap.size());
  this->assertPartitionsInSync();

  const CompactID ID = URIMap[URI].PrefixElts++;
  InlineStr* PfxP = intern(Pfx);
  PrefixMap[URI].push_back(PfxP);
  exi_invariant(URIMap[URI].PrefixElts == PrefixMap[URI].size());

  return {PfxP->str(), ID};
}

IDPair StringTable::addLocalName(CompactID URI, StrRef Name) {
  exi_invariant(URI < URIMap.size());
  this->assertPartitionsInSync();

  const CompactID ID = URIMap[URI].LNElts++;
  LocalName* LN = createLocalName(Name);
  LNMap[URI].push_back(LN);

  return {LN->Name, ID};
}

IDPair StringTable::addGlobalValue(StrRef Value) {
  const CompactID ID = *GValueCount;
  // Add to the global table, no other interaction needed.
  return {createGlobalValue(Value)->str(), ID};
}

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  // Saving these is ok since we know there are at least 4 inline slots in
  // the partition.
  auto Nil = createURI(""_str,  ""_str).second;
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
    LOG_EXTRA("Created <xmlns:{}=\"{}\">", PfxP->str(), Interned);
  }

  // Create space for the new LocalName.
  LNMap.resize(*++LNCount);

  return {URIPart, ID};
}

void StringTable::appendLocalNames(CompactID ID, ArrayRef<StrRef> LocalNames) {
  exi_invariant(ID < *LNCount);
  URIInfo& Info = URIMap[ID];
  LNMapType& NameMap = LNMap[ID];
  for (StrRef Local : LocalNames) {
    auto* LN = createLocalName(Local);
    NameMap.push_back(LN);
    ++Info.LNElts;
  }
}

} // namespace exi::decode
