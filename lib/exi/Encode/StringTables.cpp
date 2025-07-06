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

#include <exi/Encode/StringTable.hpp>
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
using NSContext = encode::StringTable::NSContext;

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

namespace exi::encode {

EXI_COLD NSContext NSContext::Unbound(NSContext::EntryType<URIEntry> URI) {
  return NSContext {
    .URI      = X(URI.first),
    .NewURI   = URI.second
  };
}
NSContext NSContext::New(NSContext::EntryType<URIEntry> URI,
                         NSContext::EntryType<PrefixEntry> Pfx) {
  return NSContext {
    .URI        = X(URI.first),
    .Pfx        = Pfx.first,
    .NewURI     = URI.second,
    .NewPfx     = Pfx.second,
    .Anonymous  = Pfx.first->getKey().empty()
  };
}
EXI_COLD NSContext NSContext::Overwrite(NSContext::EntryType<URIEntry> URI,
                                        NSContext::EntryType<PrefixEntry> Pfx) {
  NSContext Out = NSContext::New(URI, Pfx);
  Out.Overwrites = true;
  return Out;
}

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

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
    exi_unimplemented("DatatypeRepresentationMap is unsupported.");
  }
}

//////////////////////////////////////////////////////////////////////////
// Prefixes

void StringTable::pushURIContext(PrefixEntry* EPfx, URIEntry* URI) {
  exi_invariant(EPfx);
  PrefixInfo& Pfx = *VOf(EPfx);

  exi_assert(URI && Pfx.Link);
#if EXI_INVARIANTS
  if EXI_NEVER(!Pfx.isSyncedWithURI()) {
    StrRef LinkName = Pfx.Link
      ? X(Pfx.Link)->getKey()
      : "<nullptr>";
    exi::format_fatal_error(
      "Prefix '{}' is unset in URI \"{}\"!",
        EPfx->getKey(), LinkName);
  }
#endif

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
    BindPrefixToNewURI(&Pfx, URI);
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
  Pfx.syncWithURI();
  exi_assert(Pfx.isSyncedWithURI());
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

NSContext StringTable::createURI(CachedHashStrRef URI, Option<StrRef> Pfx) {
  auto [UE, IsNewURI] = createURIOnly(URI);
  if (!Pfx)
    return NSContext::Unbound({UE, IsNewURI});

  auto [It, IsNewPfx] = PrefixMap.try_emplace(*Pfx);
  if (IsNewPfx) {
    BindPrefixToNewURI(&It->second, UE);
    return NSContext::New(
      {UE, IsNewURI}, {&*It, IsNewPfx});
  } else {
    pushURIContext(&*It, UE);
    return NSContext::Overwrite(
      {UE, IsNewURI}, {&*It, IsNewPfx});
  }
}

//////////////////////////////////////////////////////////////////////////
// Batch Initialization

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  auto Nil = createURI(""_str,  ""_str);
  auto Xml = createURI(XML_URI, "xml"_str);
  auto Xsi = createURI(XSI_URI, "xsi"_str);
  // TODO: Setup predefined URIs

  // D.3 - Initial Entries in LocalName Partitions
  appendLocalNames(Xml.uri(), XML_InitialValues);
  appendLocalNames(Xsi.uri(), XSI_InitialValues);

  if (UsesSchema) {
    // TODO: When a schema is provided, prepopulate with the LocalName of each
    // attribute, element and type explicitly declared in the schema.
    auto Xsd = createURI(XSD_URI);
    appendLocalNames(Xsd.uri(), XSD_InitialValues);
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
