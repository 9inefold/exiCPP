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
#define GEN_IV(URI, PFX, LV) #LV
#define SEP ,

// TODO: Merge common info...

using namespace exi;
using NSContext = encode::StringTable::NSContext;

namespace {
enum : u64 { kDefaultReserveSize = 64 };

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

namespace exi::encode {

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

#define CHECK_ALIGNMENT(REAL, FAKE)                                           \
  static_assert(exi::PointerLikeTypeTraits<REAL*>::NumLowBitsAvailable >=     \
                exi::PointerLikeTypeTraits<FAKE*>::NumLowBitsAvailable,       \
                #FAKE " is overaligned! This should never occur, please report.");

StringTable::StringTable()
    : NameCache(Alloc), URIMap(4, Alloc),
      PrefixMap(4, Alloc), LVMap(), GValueMap(kDefaultReserveSize) {
  CHECK_ALIGNMENT(PrefixEntry, STPrefixEntry);
  CHECK_ALIGNMENT(URIEntry,    STURIEntry);
  CHECK_ALIGNMENT(ValueEntry,  STValueEntry);
  CHECK_ALIGNMENT(InlineStr,   QualName);
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
    exi_todo("schemas are currently unsupported.");
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
  // FIXME: Do nothing if Pfx.Link == X(URI)
#if EXI_INVARIANTS
  if EXI_NEVER(!Pfx.isSyncedWithURI()) {
    StrRef LinkName = Pfx.Link
      ? X(Pfx.Link)->getKey()
      : "<nullptr>";
    exi::format_fatal_error(
      "Prefix '{}' is unset in current URI \"{}\"!",
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

std::pair<StringTable::URIEntry*, bool>
 StringTable::createURIOnly(CachedHashStrRef URI) {
  auto [It, DidInsert] = URIMap->try_emplace(URI);
  if (DidInsert) {
    URIMap.recalculateLog();
    // Since the URI was already inserted, decrement.
    const u32 NewURI = URIMap->size() - 1;
    if EXI_NEVER(NewURI > kURIMax)
      Throw<range_error>("Exceeded the maximum number of URIs!");
    It->second.URI = NewURI;
  }
  return {&*It, DidInsert};
}

NSContext StringTable::createURI(CachedHashStrRef URI,
                                 Option<CachedHashStrRef> Pfx) {
  XEntry<URIEntry> UEntry = createURIOnly(URI);
  NSContext Ctx(UEntry);
  if (!Pfx)
    return Ctx.Unbound();

  auto [PI, IsNewPfx] = createPfxOnly(*Pfx);
  Ctx.Prefix(PI, IsNewPfx).Anonymous(PI == Pfx_NIL);
  if (IsNewPfx) {
    BindPrefixToNewURI(&PI->second, UEntry.data());
    return Ctx;
  } else {
    pushURIContext(PI, UEntry.data());
    return Ctx.Overwrite(true);
  }
}

NSContext StringTable::createURIForInit(StrRef URI, StrRef Pfx) {
  XEntry<URIEntry> UEntry = createURIOnly(Hash(URI));
  NSContext Ctx(UEntry);

  auto [It, IsNewPfx] = PrefixMap.try_emplace(Pfx);
  if (!IsNewPfx)
    ThrowDyn<argument_error>(
      "Duplicate prefix '"_twine + Pfx + "' during initialization.");

  PrefixEntry* PI = &*It;
  Ctx.Prefix(PI, true).Anonymous(Pfx.empty());
  BindPrefixToNewURI(&PI->second, UEntry.data());
  return Ctx;
}

LocalNameInfo* StringTable::createNewLocalName(URIEntry* URI, StrRef Raw) {
  const InlineStr* Name = internLocalName(Raw);
  auto* LN = new (Alloc) LocalNameInfo(X(URI), Name);
#if EXI_DEBUG
  auto* Alt = LVMap.GetOrInsertNode(LN);
  if EXI_UNLIKELY(LN != Alt) {
    LOG_ERROR("LocalName '{}:{}' was already inserted!", URI->first(), Raw);
    std::destroy_at(LN);
    Alloc.Deallocate(LN);
    return Alt;
  }
#else
  LVMap.InsertNode(LN);
#endif
  return LN;
}

LocalNameInfo* StringTable::getLocalName(URIEntry* URI, StrRef Raw) {
  exi_invariant(URI != nullptr);
  FoldingSetNodeID ID;
  ID.AddString(Raw);
  ID.AddInteger(VOf(URI)->uri());
  void* InsertPoint;
  auto* LN = LVMap.FindNodeOrInsertPos(ID, InsertPoint);
  // We have to make a new LocalName.
  if (LN == nullptr) {
    const InlineStr* Name = internLocalName(Raw);
    LN = new (Alloc) LocalNameInfo(X(URI), Name);
    LVMap.InsertNode(LN, InsertPoint);
  }
  return LN;
}

//////////////////////////////////////////////////////////////////////////
// Batch Initialization

void StringTable::createInitialEntries(bool UsesSchema) {
  // D.1 & D.2 - Initial Entries in Uri & Prefix Partition
  // FIXME: Move these to the constructor?
  auto Nil = createURIForInit(""_str,  ""_str);
  auto Xml = createURIForInit(XML_URI, "xml"_str);
  auto Xsi = createURIForInit(XSI_URI, "xsi"_str);

  this->Pfx_NIL = Nil.Pfx;
  this->Pfx_xml = Xml.Pfx;
  this->Pfx_xsi = Xsi.Pfx;

  // D.3 - Initial Entries in LocalName Partitions
  appendLocalNames(Xml.URI, XML_InitialValues);
  appendLocalNames(Xsi.URI, XSI_InitialValues);

  if (UsesSchema) {
    // TODO: When a schema is provided, prepopulate with the LocalName of each
    // attribute, element and type explicitly declared in the schema.
    auto Xsd = createURI(XSD_URI);
    this->Pfx_xsd = Xsd.Pfx;
    appendLocalNames(Xsd.URI, XSD_InitialValues);
  }
}

void StringTable::appendLocalNames(
 URIEntry* ID, ArrayRef<StrRef> LNMappings) {
  exi_relassert(ID != nullptr);
  for (auto Local : LNMappings) {
    const auto* LN = createNewLocalName(ID, Local);
    exi_relassert(LN != nullptr);
  }
}

} // namespace exi::encode
