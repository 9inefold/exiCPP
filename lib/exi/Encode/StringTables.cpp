//===- exi/Encode/StringTables.cpp ----------------------------------===//
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
#include <core/Common/STLExtras.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/D/ExiInitialValues.impl>
#include <algorithm>

#define DEBUG_TYPE "StringTables"
#define GEN_IV(URI, PFX, LV) {                                                \
  .LocalName = #LV, .QualifiedName = #URI "$" #LV                             \
}
#define SEP ,

// TODO: Merge common info...

using namespace exi;

using NameMapping = encode::StringTable::NameMapping;

namespace {
enum : u64 { kDefaultReserveSize = 64 };

constexpr StrRef XML_URI("http://www.w3.org/XML/1998/namespace");
constexpr NameMapping XML_InitialValues[] { EXI_XML_IV(GEN_IV, SEP) };

constexpr StrRef XSI_URI("http://www.w3.org/2001/XMLSchema-instance");
constexpr NameMapping XSI_InitialValues[] { EXI_XSI_IV(GEN_IV, SEP) };

constexpr StrRef XSD_URI("http://www.w3.org/2001/XMLSchema");
constexpr NameMapping XSD_InitialValues[] { EXI_XSD_IV(GEN_IV, SEP) };

} // namespace `anonymous`

static const Option<String&> PullSchemaID(const Option<MaybeBox<String>>& ID) {
  return ID.expect("schema should resolve to value or nil").get();
}

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

namespace exi::encode {

StringTable::StringTable()
    : NameCache(Alloc), URIMap(4, Alloc),
      PrefixMap(4, Alloc), LVMap(4), GValueMap(kDefaultReserveSize) {}

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
    LOG_WARN("Bounded tables are not supported.");
  }

  if (Opts.DatatypeRepresentationMap) {
    // TODO: DatatypeRepresentationMap?
    exi_unimplemented("datatype mapping is unsupported.");
  }
}

//////////////////////////////////////////////////////////////////////////
// Prefixes

void StringTable::pushURIContext(PrefixEntry* EPfx, URIEntry* URI) {
  exi_invariant(EPfx);
  PrefixInfo& Pfx = *VOf(EPfx);

  exi_assert(URI && Pfx.Link);
  exi_invariant(VOfX(Pfx.Link)->contains(&Pfx),
               "Prefix is unset in current link!");

  bool WillInsert = false;
  // Don't cleanup if map was just lazily initialized.
  if EXI_LIKELY(!URIStackMap.empty()) {
    if (!URIStackMap.contains(EPfx)) {
      WillInsert = true;
      this->cleanupURIStacks();
    }
  }

  URIStackMap[EPfx].emplace_back(Pfx);
  if EXI_UNLIKELY(WillInsert)
    URIStackMap.updateCacheIfOutOfDate();
  // Only remap if the value is different.
  if (Pfx.Link != X(URI))
    Pfx = MakePrefix(&Pfx, URI);
}

void StringTable::popURIContext(PrefixEntry* EPfx) {
  exi_assert(EPfx);
  PrefixInfo& Pfx = *VOf(EPfx);

#if EXI_ASSERTS
  if EXI_NEVER(!URIStackMap->contains(EPfx)) {
    exi::format_fatal_error(
      "Prefix '{}' was never pushed.", EPfx->getKey());
  }
#endif

  URIStack& TheStack = URIStackMap[EPfx];
  if EXI_NEVER(TheStack.empty()) {
    exi::format_fatal_error(
      "Prefix '{}' has no saved context.", EPfx->getKey());
  }

  Pfx = TheStack.pop_back_val();
  exi_assert(VOfX(Pfx.Link)->contains(&Pfx));
}

void StringTable::URIStackMapHandler::cleanupUnusedStacks() {
  if (empty())
    return;
  // Stack is known to be initialized.
  auto I = TheStacks->begin();
  auto E = TheStacks->cend();
  // Remove all empty stacks.
  for (; I != E; ++I) {
    const PrefixEntry* Key = I->first;
    if (!I->second.empty())
      continue;
    uncache(Key);
    TheStacks->erase(I);
  }
  // Fix up any potential cache mishaps...
  // TODO: Check if necessary?
  this->updateCacheIfOutOfDate();
}

void StringTable::cleanupURIStacks() {
  if (URIStackMap.empty())
    return;
  if EXI_LIKELY(!GuessIfMapIsReallocating(*URIStackMap))
    // The map is very unlikely to reallocate.
    return;
  // Last ditch effort to avoid reallocating...
  URIStackMap.cleanupUnusedStacks();
}

//////////////////////////////////////////////////////////////////////////
// Uniquing

const QualName* StringTable::internQualName(u32 URI, StrRef LocalName) {
  SmallStr<80> Storage;
  this->writeURITagChecked(URI, Storage);
  Storage.push_back('$');
  Storage.append(LocalName.begin(), LocalName.end());
  return X(NameCache.saveRaw(Storage.str()));
}

std::pair<StringTable::URIEntry*, StringTable::PrefixEntry*>
 StringTable::createURI(CachedHashStrRef URI, Option<StrRef> Pfx) {
  URIEntry* URIP = createURIOnly(URI).first;
  if (!Pfx)
    return {URIP, nullptr};

  auto [It, DidInsert] = PrefixMap.try_emplace(*Pfx);
  if (DidInsert) {
    auto& PI = VOf(*It);
    auto& PfxMap = VOf(URIP)->PfxMap;

    PI.Link = X(URIP);
    PI.Pfx = PfxMap.size();
    PI.PfxLog = ID_Log2<true>(u16(PI.Pfx + 1u));
    PI.WithURI = URIP->second.URI;

    PfxMap.push_back(&PI);
    return {URIP, &*It};
  }

  exi_todo("nested pfx contexts unimplemented.");
}

//////////////////////////////////////////////////////////////////////////
// Batch Initialization

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  auto* Nil = createURI(""_str,  ""_str).first;
  auto* Xml = createURI(XML_URI, "xml"_str).first;
  auto* Xsi = createURI(XSI_URI, "xsi"_str).first;

  // D.3 - Initial Entries in LocalName Partitions
  appendLocalNames(Xml, XML_InitialValues);
  appendLocalNames(Xsi, XSI_InitialValues);

  if (UsesSchema) {
    // TODO: When a schema is provided, prepopulate with the LocalName of each
    // attribute, element and type explicitly declared in the schema.
    auto* Xsd = createURI(XSD_URI).first;
    appendLocalNames(Xsd, XSD_InitialValues);
  }
}

void StringTable::appendLocalNames(
 URIEntry* ID, ArrayRef<NameMapping> LNMappings) {
  exi_relassert(ID != nullptr);
  for (auto [Local, Qualified] : LNMappings) {
    const auto* LN = internQualName(Qualified);
    this->initLocalName(ID, LN);
  }
}

void StringTable::appendLocalNames(ArrayRef<NameMapping> LNMappings) {
  for (auto [Local, Qualified] : LNMappings) {
    const auto* LN = internQualName(Qualified);
    this->initLocalName(nullptr, LN);
  }
}

void StringTable::initLocalName(URIEntry* URI, const QualName* ID) {
  exi_relassert(ID != nullptr);
  // Initializing LocalNameInfo.
  auto [It, DidEmplace] = LVMap.try_emplace(ID, URI);
  if EXI_UNLIKELY(!DidEmplace)
    LOG_WARN("\"{}\" already exists.", X(ID)->str());
}

} // namespace exi::encode
