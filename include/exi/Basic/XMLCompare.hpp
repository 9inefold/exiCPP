//===- exi/Basic/XMLCompare.hpp -------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file implements the XMLCompare class.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>
//#include <core/Common/PointerIntPair.hpp>
#include <core/Common/PointerUnion.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Basic/XML.hpp>
#include <rapidxml.hpp>

namespace exi {

template <typename T> class SmallVecImpl;

enum class MatchFailureKind : u32 {
  None,
  MismatchedName,
  MismatchedValue,
  ExtraItem,
};

struct MatchResult {
  /// The type representing a match pointer.
  using data_t = PointerUnion<const XMLNode*, const XMLAttribute*>;
  /// Pointer to the non-matching node.
  data_t Data = nullptr;
  /// Depth the error occurred at.
  u32 ErrorDepth : 30 = 0;
  /// Which "side" the error occurred on, `0` for `In`, `1` for `Out`.
  u32 Side : 1 = 0;
  /// If the error was on a node or an attribute.
  u32 IsNode : 1 = true;
  /// The actual error that occurred.
  MatchFailureKind ErrorKind = MatchFailureKind::None;
};

/// @brief Matches two XML documents.
/// @param Opts Options for ignoring parts of the XML in the output.
/// @return Whether or not the comparison was true.
bool matchXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                          ExiOptions::PreserveOpts Opts,
                          SmallVecImpl<MatchResult>& Matches);

/// @brief Matches two XML documents.
/// @param Opts Options for ignoring parts of the XML in the output.
/// @return Whether or not the comparison was true.
inline bool matchXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                                 const ExiOptions& Header,
                                 SmallVecImpl<MatchResult>& Matches) {
  return matchXMLWithPreserve(In, Out, Header.Preserve, Matches);
}

/// @brief Matches two XML documents with all options.
/// @return Whether or not the comparison was true.
inline bool matchXML(const XMLDocument* In, const XMLDocument* Out,
                     SmallVecImpl<MatchResult>& Matches) {
  static constexpr ExiOptions::PreserveOpts Opts {
    .Comments      = true,
    .DTDs          = true,
    .PIs           = true,
    .Prefixes      = true
  };
  return matchXMLWithPreserve(In, Out, Opts, Matches);
}

//////////////////////////////////////////////////////////////////////////

/// @brief Compares two XML documents.
/// @param Opts Options for ignoring parts of the XML in the output.
/// @return Whether or not the comparison was true.
bool compareXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                            ExiOptions::PreserveOpts Opts);

/// @brief Compares two XML documents.
/// @param Opts Options for ignoring parts of the XML in the output.
/// @return Whether or not the comparison was true.
inline bool compareXMLWithPreserve(const XMLDocument* In, const XMLDocument* Out,
                                   const ExiOptions& Header) {
  return compareXMLWithPreserve(In, Out, Header.Preserve);
}

/// @brief Compares two XML documents with all options.
/// @return Whether or not the comparison was true.
inline bool compareXML(const XMLDocument* In, const XMLDocument* Out) {
  static constexpr ExiOptions::PreserveOpts Opts {
    .Comments      = true,
    .DTDs          = true,
    .PIs           = true,
    .Prefixes      = true
  };
  return compareXMLWithPreserve(In, Out, Opts);
}

} // namespace exi
