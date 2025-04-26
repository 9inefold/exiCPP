//===- exi/Basic/EventCodes.cpp -------------------------------------===//
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
/// This file defines an interface for event codes used by the program.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/EventCodes.hpp>
#include <core/Common/EnumTraits.hpp>
#include <core/Common/StrRef.hpp>

using namespace exi;

static constexpr i32 kEventTermCount = EnumRange<EventTerm>::size;

static constexpr StringLiteral EventTermNames[kEventTermCount] {
  "SD", "ED",
  "SE", "SE", "SE",
  "EE",
  "AT", "AT", "AT",
  "CH", "CH",
  "NS",
  "CM",
  "PI", "DT", "ER",
  "SC"
};

static constexpr StringLiteral EventTermFullNames[kEventTermCount] {
  "SD", "ED",
  "SE (*)",
  "SE (uri:*)",
  "SE (qname)",
  "EE",
  "AT (*)",
  "AT (uri:)",
  "AT (qname)",
  "CH",
  "CH (ext)",
  "NS",
  "CM",
  "PI", "DT", "ER",
  "SC"
};

static constexpr StringLiteral EventTermMessages[kEventTermCount] {
  "Start Document",
  "End Document",
  "Start Element (*)",
  "Start Element (uri:*)",
  "Start Element (qname)",
  "End Element",
  "Attribute (*, value)",
  "Attribute (uri:*, value)",
  "Attribute (qname, value)",
  "Characters (value)",
  "Characters (external-value)",
  "Namespace Declaration (uri, prefix, local-element-ns)",
  "Comment text (text)",
  "Processing Instruction (name, text)",
  "DOCTYPE (name, public, system, text)",
  "Entity Reference (name)",
  "Self Contained"
};

StrRef exi::get_event_name(EventTerm E) noexcept {
  const i32 Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kEventTermCount && Ix >= 0)
    return EventTermNames[Ix].data();
  return "??"_str;
}

StrRef exi::get_event_fullname(EventTerm E) noexcept {
  const i32 Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kEventTermCount && Ix >= 0)
    return EventTermFullNames[Ix].data();
  return "??"_str;
}

StrRef exi::get_event_signature(EventTerm E) noexcept {
  const i32 Ix = static_cast<i32>(E);
  if EXI_LIKELY(Ix < kEventTermCount && Ix >= 0)
    return EventTermMessages[Ix].data();
  return "Unknown Event"_str;
}
