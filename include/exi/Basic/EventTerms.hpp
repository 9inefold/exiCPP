//===- exi/Basic/EventTerms.hpp -------------------------------------===//
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
/// This file defines the terminal symbols used by the program.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>

namespace exi {

class StrRef;
/// The underlying type for EventTerms.
using EventTermType = i32;

/// An enum containing all the terminal symbols used for productions.
enum class EventTerm : EventTermType {
  SD,       // Start Document
  ED,       // End Document
  SE,       // Start Element (*)
  SEUri,    // Start Element (uri:*)
  SEQName,  // Start Element (qname)
  EE,       // End Element
  AT,       // Attribute (*, value)
  ATUri,    // Attribute (uri:*, value)
  ATQName,  // Attribute (qname, value)
  CH,       // Characters (value)
  CHExtern, // Characters (external-value)
  NS,       // Namespace Declaration (uri, prefix, local-element-ns)
  CM,       // Comment text (text)
  PI,       // Processing Instruction (name, text)
  DT,       // DOCTYPE (name, public, system, text)
  ER,       // Entity Reference (name)
  SC,       // Self Contained
  Void,
  SEAny = SE,
  ATAny = AT,
  Last = SC,
  Invalid = 0b1111111,
};

/// An enum containing a simplified set of the terminal symbols.
/// Ordered so goto tables are more efficient.
enum class SimpleEventTerm : EventTermType {
  SE,       // Start Element (any)
  EE,       // End Element
  AT,       // Attribute (any, value)
  CH,       // Characters (any-value)
  NS,       // Namespace Declaration (uri, prefix, local-element-ns)
  CM,       // Comment text (text)
  PI,       // Processing Instruction (name, text)
  DT,       // DOCTYPE (name, public, system, text)
  ER,       // Entity Reference (name)
  SD,       // Start Document
  ED,       // End Document
  SC,       // Self Contained
  HL,       // Halt - internal use only
  Last = SC,
  Invalid = HL,
};

StrRef get_event_name(EventTerm E) EXI_READNONE;
StrRef get_event_fullname(EventTerm E) EXI_READNONE;
StrRef get_event_signature(EventTerm E) EXI_READNONE;

StrRef get_event_name(SimpleEventTerm E) EXI_READNONE;
StrRef get_event_fullname(SimpleEventTerm E) EXI_READNONE;
StrRef get_event_signature(SimpleEventTerm E) EXI_READNONE;

} // namespace exi
