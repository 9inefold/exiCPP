//===- exi/Basic/XMLManager.hpp -------------------------------------===//
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

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/EnumTraits.hpp>
#include <core/Common/IntrusiveRefCntPtr.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Support/Error.hpp>
// #include <exi/Basic/FileManager.hpp>
#include <exi/Basic/XML.hpp>

namespace exi {

class MemoryBuffer;
class WritableMemoryBuffer;
class XMLContainer;
class raw_ostream;

class XMLManager : public ThreadSafeRefCountedBase<XMLManager> {
  Option<XMLOptions> DefaultOpts;
  SpecificBumpPtrAllocator<XMLContainer> FilesAlloc;

  xml::XMLBumpAllocator SharedDocAlloc;
  StringMap<XMLContainer*, BumpPtrAllocator> SeenFiles;

  /// Allocates the base container with `FilesAlloc`.
  XMLContainer* allocateContainer(bool SharedAlloc = true);

  Expected<XMLDocument&> getXMLDocumentImpl(StrRef Filepath,
                                            bool IsVolatile = false);

public:
  XMLManager(Option<XMLOptions> Opts = std::nullopt);
  ~XMLManager();

  Expected<XMLDocument&> getXMLDocument(const Twine& Filepath,
                                        bool IsVolatile = false);
  
  /// Get a `XMLDocument&` if it exists, printing to `OS` on error.
  Option<XMLDocument&>
   getOptionalXMLDocument(const Twine& Filepath, raw_ostream& OS);

  /// Get a `XMLDocument&` if it exists, without doing anything on error.
  Option<XMLDocument&>
   getOptionalXMLDocument(const Twine& Filepath) {
    return expectedToOptional(
      getXMLDocument(Filepath, false));
  }
};

} // namespace exi
