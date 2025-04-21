//===- exi/Grammar/Grammar.hpp --------------------------------------===//
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
/// This file defines the base for grammars.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/Result.hpp>
#include <core/Common/SmallVec.hpp>
#include <exi/Basic/EventCodes.hpp>

namespace exi {

class ExiDecoder;
class ExiEncoder;

/// Represents the first part of an event code.
using FirstLevelProd = u64;
/// Value is a full event code, error is the first part of an event code.
using GrammarTerm = Result<EventQName, FirstLevelProd>;

/// The base for all grammars.
class Grammar {
public:
  /// Gets the terminal symbol at the current position, if it exists.
  /// Otherwise returns the first part of the event code.
  virtual GrammarTerm getTerm(ExiDecoder& Decoder) = 0;
  virtual void dump() const {}
  virtual ~Grammar() = default;
private:
  virtual void anchor();
};

/// The grammars for `BuiltinSchema`.
/// TODO: Profile SmallVecs
class BuiltinGrammar final : public Grammar {
  /// One inline element for StartElement.
  SmallVec<SmallQName, 1> StartTag;
  /// One inline element for StartElement or CHaracters.
  SmallVec<SmallQName, 1> Element;

public:
  /// Gets the terminal symbol at the current position.
  GrammarTerm getTerm(ExiDecoder& Decoder) override;
  void dump() const override;
};

} // namespace exi
