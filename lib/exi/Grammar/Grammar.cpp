//===- exi/Grammar/Grammar.cpp --------------------------------------===//
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

#include <exi/Grammar/Grammar.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/CompactID.hpp>
#include "SchemaGet.hpp"

#define DEBUG_TYPE "Grammar"

using namespace exi;

void Grammar::anchor() {}

static u64 ReadBits(OrdReader& Strm, u32 Bits) {
  auto Out = Strm->readBits64(Bits);
  if EXI_UNLIKELY(Out.is_err())
    exi_unreachable("invalid stream read.");
  return *Out;
}

GrammarTerm BuiltinGrammar::getTerm(OrdReader& Strm, bool IsStart) {
  auto& Elts = this->getElts(IsStart);
  const usize Size = Elts.size();
  const u32 Bits = this->getLog(IsStart);
  const u64 Out = ReadBits(Strm, Bits);
  // Check if this is a valid offset.
  if (Out < Size) {
    // Values are always pushed in reverse order, so remap the position.
    const auto Pos = (Size - 1) - Out;
    // const auto Pos = Out;
    LOG_EXTRA("Code[0]*: @{}:{}", Bits, Out);
    return Ok(Elts[Pos]);
  }

  LOG_EXTRA("Code[0]: @{}:{}", Bits, Out);
  // Get the base event code offset.
  return Err(Out - Size);
}

void BuiltinGrammar::addTerm(EventUID Term, bool IsStart) {
  getElts(IsStart).push_back(Term);
  this->setLog(IsStart);
}

void BuiltinGrammar::setLog(bool IsStart) {
  if (IsStart) {
    const u32 Log = getStartTagCount();
    StartTagLog = CompactIDLog2(Log);
  } else {
    const u32 Log = getElementCount();
    ElementLog = CompactIDLog2(Log);
  }
}

void BuiltinGrammar::dump(ExiDecoder* D) const {
  outs() << "StartTag:\n";
  for (auto [Ix, Val] : exi::enumerate(exi::reverse(this->StartTag))) {
    outs() << "  " << get_event_name(Val.getTerm())
      << "  " << Ix << '\n';
  }

  // outs() << "Element\n";
}
