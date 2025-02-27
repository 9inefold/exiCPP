//===- exi/Basic/XMLManager.cpp -------------------------------------===//
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
///
/// \file
/// This file defines a handler for XML files.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/XMLManager.hpp>
#include <core/Common/SmallStr.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Filesystem.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/MemoryBuffer.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/Path.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/XMLContainer.hpp>

#define DEBUG_TYPE "XMLManager"

using namespace exi;

XMLManager::XMLManager(Option<XMLOptions> Opts) :
 DefaultOpts(Opts), SeenFiles(4) {
}

XMLManager::~XMLManager() = default;

XMLContainer* XMLManager::allocateContainer(bool SharedAlloc) {
  auto* Alloc = SharedAlloc ? &SharedDocAlloc : nullptr;
  return new (FilesAlloc.Allocate()) XMLContainer(DefaultOpts, Alloc);
}

Expected<XMLDocument&>
 XMLManager::getXMLDocumentImpl(StrRef Filepath, bool IsVolatile) {
  exi_invariant(sys::path::is_absolute(Filepath),
    "Inputs to SeenFiles must be absolute paths");
  
  auto [SeenFileEntryIt, DidInsert] =
    SeenFiles.insert({Filepath, nullptr});
  if (!DidInsert) {
    LOG_EXTRA("Getting cached file '{}'", Filepath);
    if (!SeenFileEntryIt->second)
      return createStringError("Invalid input file '{}'", Filepath);
    return SeenFileEntryIt->second->parse();
  }

  XMLContainer::MapEntry* NamedEnt = &*SeenFileEntryIt;
  XMLContainer*& Entry = NamedEnt->second;
  exi_assert(!Entry, "should be newly-created");

  Entry = this->allocateContainer(/*SharedAlloc=*/true);
  exi_assert(Entry, "should be newly-created");

  LOG_EXTRA("Parsing file '{}'", Filepath);
  return Entry->loadAndParse(*NamedEnt, IsVolatile);
}

Expected<XMLDocument&>
 XMLManager::getXMLDocument(const Twine& Filepath, bool IsVolatile) {
  SmallStr<80> Storage;
  Filepath.toVector(Storage);
  sys::fs::make_absolute(Storage);
  return getXMLDocumentImpl(Storage.str(), IsVolatile);
}

Option<XMLDocument&>
 XMLManager::getOptionalXMLDocument(const Twine& Filepath, raw_ostream& OS) {
  Expected<XMLDocument&> Result
    = getXMLDocument(Filepath, false);
  if (Result)
    return *Result;
  
  logAllUnhandledErrors(Result.takeError(), OS);
  return std::nullopt;
}
