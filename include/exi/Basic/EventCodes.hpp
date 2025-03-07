//===- exi/Basic/EventCodes.hpp -------------------------------------===//
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

#pragma once

#include <core/Common/Fundamental.hpp>
#include <core/Common/EnumArray.hpp>

namespace exi {

/// An enum containing all the terminal symbols used for productions.
enum class EventTerm : i32 {
  SD, // Start Document
  ED, // End Document
  SE, // Start Element (qname)
  EE, // End Element
  AT, // Attribute (qname, value)
  CH, // Characters (value)
  NS, // Namespace Declaration (uri, prefix, local-element-ns)
  CM, // Comment text (text)
  PI, // Processing Instruction (name, text)
  DT, // DOCTYPE (name, public, system, text)
  ER, // Entity Reference (name)
  SC, // Self Contained
  Last = SC,
};

} // namespace exi
