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
#include <exi/Stream/StreamVariant.hpp>

namespace exi {

class ExiDecoder;
class ExiEncoder;

/// Represents the first part of an event code.
using FirstLevelProd = u64;
/// Value is a full event code, error is the first part of an event code.
using GrammarTerm = Result<EventUID, FirstLevelProd>;

/// The base for all grammars.
class Grammar {
public:
  /// Gets the terminal symbol at the current position, if it exists.
  /// Otherwise returns the first part of the event code.
  virtual GrammarTerm getTerm(StreamReader& Strm, bool IsStart) = 0;

  /// Adds a new StartTag term to the list.
  virtual void addTerm(EventUID Term, bool IsStart) = 0;

  /// Dumps the current grammar.
  virtual void dump() const {}
  virtual ~Grammar() = default;

private:
  virtual void anchor();
};

/// The grammars for `BuiltinSchema`.
/// TODO: Profile SmallVecs
class BuiltinGrammar final : public Grammar {
  u32 StartTagLog = 0, ElementLog = 1;
  /// QName of the current element.
  SmallQName Name = SmallQName::MakeAny();
  /// One inline element for StartElement. +1
  SmallVec<EventUID, 1> StartTag;
  /// One inline element for StartElement or CHaracters. +2
  SmallVec<EventUID, 0> Element;


public:
  BuiltinGrammar() = default;
  explicit BuiltinGrammar(SmallQName Name) : Name(Name) {}

  GrammarTerm getTerm(StreamReader& Reader, bool IsStart) override;
  void addTerm(EventUID Term, bool IsStart) override;

  SmallQName getName() const { return Name; }
  void dump() const override;

private:
  /// Sets the log for StartTag or Element.
  void setLog(bool IsStart);

  /// Returns a precalculated log for StartTag or Element.
  u32 getLog(bool IsStart) const {
    return IsStart ? StartTagLog : ElementLog;
  }

  /// Returns the backing for StartTag or Element.
  SmallVecImpl<EventUID>& getElts(bool IsStart) {
    if (IsStart)
      return StartTag;
    else
      return Element;
  }
  /// Returns the backing for StartTag or Element.
  const SmallVecImpl<EventUID>& getElts(bool IsStart) const {
    if (IsStart)
      return StartTag;
    else
      return Element;
  }

  usize getStartTagCount() const {
    return StartTag.size() + 1;
  }
  usize getElementCount() const {
    return Element.size() + 2;
  }
};

} // namespace exi
