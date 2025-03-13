//===- exi/Basic/StringTables.hpp -----------------------------------===//
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

#pragma once

#include <core/Common/SmallVec.hpp>
#include <core/Common/StringMap.hpp>
#include <core/Common/StrRef.hpp>
#include <core/Common/TinyPtrVec.hpp>
#include <core/Support/Allocator.hpp>
#include <core/Support/ErrorHandle.hpp>
#include <core/Support/StringSaver.hpp>
#include <exi/Basic/CompactID.hpp>

namespace exi {

struct ExiOptions;

//===----------------------------------------------------------------===//
// Decoding
//===----------------------------------------------------------------===//

/// Defines utilities for decoding EXI.
namespace decode {

class StringTable;

/// Maps `CompactID -> T`. SmallVec typedef for simple replacement.
template <typename T, usize InlineEntries = 0>
using CIDMap = SmallVec<T, InlineEntries>;

/// The value stored for each entry in the LocalName map.
struct QName {
  StrRef LocalName; /// namespace:[local-name]
  InlineStr* FullName; /// [namespace:local-name]
  TinyPtrVec<InlineStr*> LocalValues;
};

class StringTable {
  /// Used to unique strings for output.
  OwningStringSaver NameCache;
  /// Used to page QNames?
  SpecificBumpPtrAllocator<QName> QNameAlloc;

  /// The small size, accounting for simple schemas.
  static constexpr usize kSchemaElts = 4;

  /// Used to map URI indices to strings.
  CIDMap<StrRef, kSchemaElts> URIMap;
  /// Used to map URI indices to LocalNames.
  ///  Eg. `LNMap[URI][LocalID]->LocalValues[ValueID]`
  CIDMap<CIDMap<QName*>, kSchemaElts> LNMap;

  /// Used to map LocalName IDs to GlobalValues.
  ///  Eg. `GValueMap[GlobalID]`
  CIDMap<InlineStr*> GValueMap;
};

} // namespace decode

//===----------------------------------------------------------------===//
// Encoding
//===----------------------------------------------------------------===//

/// Defines utilities for encoding EXI.
namespace encode {

class StringTable;

class StringTable {
  /// The allocator shared internally.
  exi::BumpPtrAllocator Alloc;
  /// Used to unique strings for lookup.
  UniqueStringSaver NameCache;

  // TODO: Finish design...
};

} // namespace encode

} // namespace exi
