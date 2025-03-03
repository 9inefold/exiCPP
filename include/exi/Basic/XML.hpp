//===- exi/Basic/XML.hpp --------------------------------------------===//
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
/// This file defines the interface for XML.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/StrRef.hpp>
#include <core/Common/D/Char.hpp>
#include <rapidxml_fwd.hpp>

namespace exi {

using XMLDocument  = xml::XMLDocument<Char>;
using XMLAttribute = xml::XMLAttribute<Char>;
using XMLBase      = xml::XMLBase<Char>;
using XMLNode      = xml::XMLNode<Char>;
using xml::NodeKind;

class Twine;
class XMLContainer;
class XMLManager;

enum class XMLKind : unsigned {
  Document = 0,     // An XML Document.
  XmlDocument = 0,  // An XML Document.
  ExiDocument,      // An EXI Document.

  XsdExiSchema,     // An XSD Schema in EXI form.
  XsdXmlSchema,     // An XSD Schema in XML form.
  DTDSchema,        // A DTD Schema.

  UnknownSchema,    // A schema of unknown type, deduced.
  Unknown,          // Unknown document type.
};

struct XMLOptions {
  /// If the source text will be modified.
  bool Immutable = false;
  /// Disables comment, DOCTYPE, and PI parsing.
  bool Strict = false;
};

/// Classifies paths by their extension.
/// @param PathOrExt The path or extension of a file.
/// @param Hint When extensions are ambiguous, this will decide if schema should
/// be the hinted type.
XMLKind classifyXMLKind(StrRef PathOrExt, bool HintSchema = false);

/// @overload
XMLKind classifyXMLKind(const Twine& PathOrExt, bool HintSchema = false);

} // namespace exi
