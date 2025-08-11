//===- exi/Encode/Grammar.hpp ---------------------------------------===//
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
/// This file defines the base for decoder grammars.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/Result.hpp>
#include <core/Common/SmallVec.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Stream/OrderedWriter.hpp>

#define DEBUG_TYPE "Grammar"

namespace exi {
class ExiEncoder;

namespace encode {

/// Represents the first part of an event code.
using FirstLevelProd = u64;
/// Value is a full event code, error is the first part of an event code.
using GrammarTerm = Result<EventUID, FirstLevelProd>;

/// The base for all grammars.
class alignas(8) Grammar {};

/// The grammars for `BuiltinSchema`.
/// TODO: Profile SmallVecs
class BuiltinGrammar final : public Grammar {
  u32 StartTagLog = 0, ElementLog = 1;

public:
  BuiltinGrammar() = default;
  //explicit BuiltinGrammar(SmallQName Name) : Name(Name) {}

  void dump(ExiDecoder* D) const;

private:
  usize getStartTagCount() const {
    //return StartTag.size() + 1;
  }
  usize getElementCount() const {
    //return Element.size() + 2;
  }
};

} // namespace encode
} // namespace exi

#undef DEBUG_TYPE
