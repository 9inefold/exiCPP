//===- exi/Encode/StringTables.hpp ----------------------------------===//
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
#include <core/Common/Twine.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <algorithm>

#define DEBUG_TYPE "StringTables"

using namespace exi;

namespace {
enum : u64 { kDefaultReserveSize = 64 };

constexpr StrRef XML_URI("http://www.w3.org/XML/1998/namespace");
constexpr StrRef XML_InitialValues[] { "base", "id", "lang", "space" };

constexpr StrRef XSI_URI("http://www.w3.org/2001/XMLSchema-instance");
constexpr StrRef XSI_InitialValues[] { "nil", "type" };

constexpr StrRef XSD_URI("http://www.w3.org/2001/XMLSchema");
constexpr StrRef XSD_InitialValues[] {
  "ENTITIES", "ENTITY", "ID", "IDREF", "IDREFS",
  "NCName", "NMTOKEN", "NMTOKENS", "NOTATION", "Name", "QName",
  "anySimpleType", "anyType", "anyURI",
  "base64Binary", "boolean", "byte",
  "date", "dateTime", "decimal", "double", "duration", "float",
  "gDay", "gMonth", "gMonthDay", "gYear", "gYearMonth",
  "hexBinary", "int", "integer", "language", "long",
  "negativeInteger", "nonNegativeInteger", "nonPositiveInteger",
  "normalizedString", "positiveInteger", "short", "string", "time", "token",
  "unsignedByte", "unsignedInt", "unsignedLong", "unsignedShort"
};

} // namespace `anonymous`

static const Option<String&> PullSchemaID(const Option<MaybeBox<String>>& ID) {
  return ID.expect("schema should resolve to value or nil").get();
}

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

namespace exi::encode {

StringTable::StringTable()
    : NameCache(Alloc), URIMap(4, Alloc), PrefixMap(4, Alloc) {
  //GValueMap.reserve(kDefaultReserveSize);
}

void StringTable::setup(const ExiOptions& Opts) {
  // TODO: Implement encoder setup...
  exi_unreachable("implement setup");
}

} // namespace exi::encode
