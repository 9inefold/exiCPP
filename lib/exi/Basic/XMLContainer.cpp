//===- exi/Basic/XMLContainer.cpp -----------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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

#include <exi/Basic/XMLContainer.hpp>
#include <core/Common/SmallStr.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Common/STLExtras.hpp>
#include <core/Support/Alignment.hpp>
#include <core/Support/Error.hpp>
#include <core/Support/Format.hpp>
#include <core/Support/Logging.hpp>
#include <core/Support/MemoryBuffer.hpp>
#include <core/Support/MemoryBufferRef.hpp>
#include <core/Support/Path.hpp>
#include <core/Support/ScopedSave.hpp>
#include <core/Support/WithColor.hpp>
#include <rapidxml.hpp>
#include <fmt/format.h>

#define DEBUG_TYPE "xml-parsing"

using namespace exi;

namespace {
constexpr unsigned kDefaultDocKind = unsigned(XMLKind::Unknown);

#ifndef NDEBUG
constexpr int kValidate = xml::parse_validate_closing_tags;
#else
constexpr int kValidate = 0;
#endif

constexpr int kDefault = xml::parse_no_string_terminators
                       | xml::parse_no_entity_translation
                       /*| xml::parse_no_data_nodes*/
                       //| xml::parse_normalize_newlines
                       //| xml::parse_merge_cdata_nodes
                       | kValidate;

constexpr int kImmutable = kDefault
                         | xml::parse_non_destructive;

constexpr int kMergeData = kDefault
                         | xml::parse_merge_cdata_nodes;
} // namespace `anonymous`

XMLContainer::XMLContainer(Option<XMLParseOptions> Opts, 
                           Option<xml::XMLBumpAllocator&> Alloc) :
 TheDocument(Alloc), DocKind(kDefaultDocKind), Parsed(false) {
  this->setOptions(Opts);
}
          
XMLContainer::~XMLContainer() = default;

MemoryBufferRef XMLContainer::getBufferRef() const {
  if (TheBuffer)
    return TheBuffer->getMemBufferRef();
  return MemoryBufferRef();
}

StrRef XMLContainer::getRelativeName() const {
  if EXI_UNLIKELY(!ME)
    return "";
  return sys::path::relative_path(ME->getKey());
}

void XMLContainer::setKind(Option<XMLKind> Kind) {
  // If an explicit type was provided, use that.
  if (Kind) {
    DocKind = exi::to_underlying(*Kind);
    return;
  }

  // If MapEntry has been set, deduce the type.
  if (ME != nullptr) {
    const auto K = classifyXMLKind(ME->getKey());
    DocKind = exi::to_underlying(K);
    return;
  }

  // Otherwise, fall back.
  DocKind = kDefaultDocKind;
}

void XMLContainer::setEntry(const MapEntry& ME, Option<XMLKind> Kind) {
#if EXI_LOGGING
  // Paths from XMLManager should always be absolute. If they aren't, something
  // went terribly wrong.
  if (!sys::path::is_absolute(ME.getKey()))
    LOG_ERROR("The MapEntry '{}' is not absolute!", ME.getKey());
#endif

  // If this entry has already been parsed, we don't want to overwrite the
  // entry. The user needs to make sure to reset the container.
  if (this->isParsed()) {
#if EXI_LOGGING
    if (const auto* Entry = this->ME)
      LOG_WARN("The XMLContainer '{}' "
               "has already been parsed!", Entry->getKey());
#endif
    return;
  }

  // Make sure this entry is actually us.
  if (this == ME.getValue()) {
    this->ME = &ME;
    this->setKind(Kind);
  }
}

template <int Base>
static void ParseWithStrict(XMLDocument& Doc, char* MBS, bool OptsStrict) {
  if (!OptsStrict)
    Doc.parse<Base | xml::parse_all>(MBS);
  else
    Doc.parse<Base>(MBS);
}

static void ParseWithQuals(XMLDocument& Doc, WritableMemoryBuffer& MB,
                           XMLParseOptions Opts) {
  char* MBS = MB.getBufferStart();
  exi_relassert(MBS && MB.getBufferSize());

  if (Opts.Immutable)
    LOG_WARN("'XMLParseOptions.Immutable' has been deprecated and is ignored.");

  if (!Opts.MergeData)
    ParseWithStrict<kDefault>(Doc, MBS, Opts.Strict);
  else
    ParseWithStrict<kMergeData>(Doc, MBS, Opts.Strict);
}

static String FormatParseError(MemoryBuffer& MB, const xml::parse_error& Ex,
                               bool Color = false) {
  const char* const OffPtr = Ex.where<Char>();
  usize Off = MB.getBufferOffset(OffPtr);
  if (Off == StrRef::npos)
    return Ex.what();
  
  StrRef Str = MB.getBuffer();
  Str = Str.rtrim(" \n\r\t\v\0");
  /// Eat BOM and adjust Offset
  if (Str.consume_front("\xEF\xBB\xBF"))
    Off -= 3;
  
  usize End = StrRef::npos;
  if (Off < Str.size())
    End = Str.find_first_of("\n\r", Off);
  Str = Str.substr(0, End);

  usize Start = Str.rfind('\n', Off);
  usize Beg = Start;
  if (Start == StrRef::npos)
    Start = 0;
  else
    Start += 1;
  
  StrRef Line = Str.substr(Start);
  SmallVec<StrRef, 4> Lines;
  Lines.push_back(Line);
  Str = Str.take_front(Beg);

 {
  int LineCount = 3;
  while (Beg != StrRef::npos && LineCount) {
    LineCount -= 1;
    Str = Str.rtrim("\n\r\t\v\0");
    Beg = Str.rfind('\n');
    Start = (Beg == StrRef::npos) ? 0 : Beg + 1;
    Lines.push_back(Str.substr(Start));
    Str = Str.take_front(Beg);
  }
 }

  String Out;
  raw_string_ostream OS(Out);
  OS.enable_colors(Color);

  WithColor(OS, raw_ostream::BRIGHT_MAGENTA)
    << format("at offset {}: '{}'\n", Off, Ex.what());
  if (Lines.size() == 4 && Start != 0)
    OS << " ... \n";
  for (auto L : exi::reverse(Lines))
    OS << L << '\n';
  // FIXME: Use utf8 distance?
  OS << indent(OffPtr - Line.data()) << '^';

  return Out;
}

Error exi::parseXMLFromBuffer(XMLDocument& Doc, WritableMemoryBuffer& MB,
                              XMLParseOptions Opts) {
  LOG_EXTRA("Parsing '{}'.", MB.getBufferIdentifier());
  try {
    // FIXME: Make this an atomic increment in threaded mode.
    ScopedSave S(xml::use_exceptions_anyway, true);
    // Try to parse with the container's qualifiers.
    ParseWithQuals(Doc, MB, Opts);
    return Error::success();
  } catch (const std::exception& Ex) {
    LOG_ERROR("Failed to parse file '{}'", MB.getBufferIdentifier());
#if !RAPIDXML_NO_EXCEPTIONS
    /// Check if it's rapidxml's wee type
    if (auto* PEx = dynamic_cast<const xml::parse_error*>(&Ex)) {
      LOG_EXTRA("Error type is 'xml::parse_error'");
      return createStringError(FormatParseError(MB, *PEx));
    }
#endif
    return createStringError(Ex.what());
  }
}

Error exi::parseXMLFromBuffer(XMLDocument& Doc, WritableMemoryBuffer& MB,
                              bool Immutable, bool Strict) {
  return exi::parseXMLFromBuffer(Doc, MB, XMLParseOptions {
    .Immutable  = Immutable,
    .Strict     = Strict 
  });
}

Error exi::parseXMLFromBuffer(XMLDocument& Doc, MemoryBuffer& MB, bool Strict) {
  LOG_EXTRA("Parsing '{}'.", MB.getBufferIdentifier());
  try {
    // FIXME: Make this an atomic increment in threaded mode?
    ScopedSave S(xml::use_exceptions_anyway, true);
    char* MBS = const_cast<char*>(MB.getBufferStart());
    if (!Strict)
      Doc.parse<kImmutable | xml::parse_all>(MBS);
    else
      Doc.parse<kImmutable>(MBS);
    return Error::success();
  } catch (const std::exception& Ex) {
    LOG_ERROR("Failed to parse file '{}'", MB.getBufferIdentifier());
#if !RAPIDXML_NO_EXCEPTIONS
    /// Check if it's rapidxml's wee type
    if (auto* PEx = dynamic_cast<const xml::parse_error*>(&Ex)) {
      LOG_EXTRA("Error type is 'xml::parse_error'");
      return createStringError(FormatParseError(MB, *PEx));
    }
#endif
    return createStringError(Ex.what());
  }
}

Expected<XMLDocument&> XMLContainer::parse() const {
  // The parse results were cached.
  if (isParsed())
    return TheDocument;
  if (!TheBuffer)
    return makeError("Buffer was not loaded!");
  if (!isXMLKind())
    return makeError("Not an xml file!");

  LOG_EXTRA("Parsing '{}'.", ME->getKey());
  try {
    // FIXME: Make this an atomic increment in threaded mode.
    ScopedSave S(xml::use_exceptions_anyway, true);
    // Try to parse with the container's qualifiers.
    ParseWithQuals(TheDocument, *TheBuffer, {
      .Immutable  = bool(Immutable),
      .Strict     = bool(Strict),
      .MergeData  = bool(MergeData)
    });
    this->Parsed = true;
    return TheDocument;
  } catch (const std::exception& Ex) {
    LOG_ERROR("Failed to parse file '{}'", ME->getKey());
#if !RAPIDXML_NO_EXCEPTIONS
    /// Check if it's rapidxml's wee type
    if (auto* PEx = dynamic_cast<const xml::parse_error*>(&Ex)) {
      LOG_EXTRA("Error type is 'xml::parse_error'");
      return makeError(FormatParseError(*TheBuffer, *PEx));
    }
#endif
    return makeError(Ex.what());
  }
}

/// @brief Parses the loaded buffer and returns a reference to the doc.
Option<XMLDocument&> XMLContainer::parseOpt() const {
  return expectedToOptional(this->parse());
}

Expected<XMLDocument&> XMLContainer::loadAndParse(const MapEntry& ME,
                                                  bool IsVolatile) {
  // In this case, we ensure the MapEntry has been initialized,
  // load the buffer, and then parse it.
  this->setEntry(ME);
  if (!TheBuffer || IsVolatile) {
    auto Result = loadBuffer(IsVolatile);
    if (Error E = Result.takeError())
      return E;
  }

  return this->parse();
}

Expected<MemoryBufferRef> XMLContainer::loadBuffer(const MapEntry& ME,
                                                   bool IsVolatile) {
  this->setEntry(ME);
  return loadBuffer(IsVolatile);
}

Expected<MemoryBufferRef> XMLContainer::loadBuffer(bool IsVolatile) const {
  if (this->isParsed())
    return makeError("loadBuffer() called on parsed entry!");
  if (ME == nullptr)
    return makeError("MapEntry was not provided!");
  
  // Check if the buffer has already been loaded. If it has, check if the file
  // is volatile (and therefore likely to have changed).
  if (!TheBuffer || IsVolatile) {
    auto Buffer = WritableMemoryBuffer::getFileEx(
      ME->getKey(), /*RequiresNullTerminator=*/true,
      IsVolatile, /*UTF32*/Align::Constant<4>());

    if (std::error_code EC = Buffer.getError())
      return makeError(EC);

    if EXI_UNLIKELY(!Buffer->get()) {
      /// Clear the buffer, this state shouldn't happen.
      TheBuffer.reset();
      LOG_ERROR("Recieved null buffer, resetting...");
      return makeError("loadBuffer() returned a null buffer!");
    }

    TheBuffer = std::move(*Buffer);
  }

  return getBufferRef();
}

Error XMLContainer::makeError(const std::error_code& EC) const {
  if (ME == nullptr)
    return errorCodeToError(EC);
  return createStringError(EC,
    format("Error in XMLContainer '{}'", getRelativeName()));
}

Error XMLContainer::makeError(const Twine& Msg) const {
  if (ME == nullptr) {
    return createStringError(
      "Error in XMLContainer: {}", Msg.str());
  }
  return createStringError(
    "Error in XMLContainer '{}': {}",
    getRelativeName(), Msg.str());
}
