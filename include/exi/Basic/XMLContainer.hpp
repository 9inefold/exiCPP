//===- exi/Basic/XMLContainer.hpp -----------------------------------===//
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
/// This file defines a wrapper for XML files used by the XMLManager class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/StringMapEntry.hpp>
#include <core/Support/Error.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <exi/Basic/XML.hpp>
#include <rapidxml.hpp>

namespace exi {

class MemoryBufferRef;
class WritableMemoryBuffer;

class alignas(8) XMLContainer {
  friend class XMLManager;
  using MapEntry = StringMapEntry<XMLContainer*>;
private:
  mutable XMLDocument TheDocument;
  mutable Box<WritableMemoryBuffer> TheBuffer;
  /// The actual entry stored in the map.
  const MapEntry* ME = nullptr;

  EXI_PREFER_TYPE(XMLKind)
  /// The `XMLKind` of the document.
  unsigned DocKind : 3;

  EXI_PREFER_TYPE(bool)
  /// If the source text was already parsed.
  mutable unsigned Parsed : 1;

  EXI_PREFER_TYPE(bool)
  unsigned Immutable : 1;

  EXI_PREFER_TYPE(bool)
  unsigned Strict : 1;

  /// Sets MapEntry and DocKind.
  /// @param Kind Allows the `XMLKind` to be forced.
  void setEntry(const MapEntry& ME,
                Option<XMLKind> Kind = std::nullopt);

  /// Sets options manually.
  void setOptions(Option<XMLOptions> Opts) {
    exi_assert(!isParsed(),
              "Options cannot be set after initialization.");
    if (Opts && !Parsed) {
      Immutable = Opts->Immutable;
      Strict = Opts->Strict;
    }
  }

  /// Sets kind, or assumes kind from Name.
  void setKind(Option<XMLKind> Kind);

public:
  XMLContainer(Option<XMLOptions> Opts,
               Option<xml::XMLBumpAllocator&> Alloc = std::nullopt);
  ~XMLContainer();

  MemoryBufferRef getBufferRef() const;
  StrRef getName() const { return ME ? ME->getKey() : ""; }
  StrRef getRelativeName() const;
  XMLKind getKind() const { return static_cast<XMLKind>(DocKind); }
  bool isParsed() const { return Parsed; }
  bool isImmutable() const { return Immutable; }
  bool isStrict() const { return Strict; }

  bool isValidKind() const {
    const XMLKind Kind = getKind();
    return Kind != XMLKind::XsdExiSchema
        && Kind != XMLKind::Unknown;
  }

  bool isSchema() const {
    const XMLKind Kind = getKind();
    return Kind != XMLKind::Document
        && Kind != XMLKind::Unknown;
  }

  /// @brief Parses the loaded buffer and returns a reference to the doc.
  Expected<XMLDocument&> parse() const;

  /// @brief Variant of parse() which loads the buffer.
  /// @param ME The MapEntry pointing to `this`.
  /// @param IsVolatile Passed to `loadBuffer` if required.
  Expected<XMLDocument&> loadAndParse(const MapEntry& ME,
                                       bool IsVolatile = false);

private:
  Expected<MemoryBufferRef> loadBuffer(const MapEntry& ME, bool IsVolatile);
  Expected<MemoryBufferRef> loadBuffer(bool IsVolatile = false) const;

  Error makeError(const std::error_code& EC) const;
  Error makeError(const Twine& Msg) const;
};

} // namespace exi
