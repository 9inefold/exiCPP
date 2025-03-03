//===- exi/Basic/XML.cpp --------------------------------------------===//
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

#include <exi/Basic/XML.hpp>
#include <core/Common/SmallStr.hpp>
#include <core/Common/StringSwitch.hpp>
#include <core/Common/Twine.hpp>
#include <core/Support/Path.hpp>
#include <core/Support/Process.hpp>
#include <core/Support/raw_ostream.hpp>
#include <rapidxml.hpp>

using namespace exi;

static XMLKind Classify(StrRef Ext, bool Hint = false) {
  using enum XMLKind;
  return StringSwitch<XMLKind>(Ext)
    .EndsWithLower("xml", XmlDocument)
    .EndsWithLower("exi", Hint ? XsdExiSchema : ExiDocument)
    .EndsWithLower("xsd", XsdXmlSchema)
    .EndsWithLower("dtd", DTDSchema)
    .Default(Hint ? UnknownSchema : Unknown);
}

XMLKind exi::classifyXMLKind(StrRef PathOrExt, bool HintSchema) {
  return Classify(PathOrExt, HintSchema);
}

static XMLKind ClassifyXMLKindTwine(const Twine& PathOrExt, bool HintSchema) {
  SmallStr<80> Storage;
  return Classify(PathOrExt.toStrRef(Storage), HintSchema);
}

XMLKind exi::classifyXMLKind(const Twine& PathOrExt, bool HintSchema) {
  if (PathOrExt.isSingleStrRef())
    return Classify(PathOrExt.getSingleStrRef(), HintSchema);
  tail_return ClassifyXMLKindTwine(PathOrExt, HintSchema);
}

//////////////////////////////////////////////////////////////////////////
// parse_error_handler

#ifdef RAPIDXML_NO_EXCEPTIONS

/// Handles throwing in different modes.
static void Throw([[maybe_unused]] const char* what,
                  [[maybe_unused]] void* where) EXI_NOEXCEPT {
#if EXI_EXCEPTIONS
# if !RAPIDXML_NO_EXCEPTIONS
  throw xml::parse_error(what, where);
# else
  throw std::runtime_error(what);
# endif
#endif
} 

void xml::parse_error_handler(const char* what, void* where) {
  if (xml::using_exceptions())
    Throw(what, where);
  errs() << "xml parse error:" << what << '\n';
  sys::Process::Exit(1); // TODO: No cleanup?
}

#endif // RAPIDXML_NO_EXCEPTIONS

volatile std::atomic<bool> xml::use_exceptions_anyway = false;
