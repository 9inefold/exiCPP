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
#include <exi/Basic/CompactID.hpp>
#include "SchemaGet.hpp"

#define DEBUG_TYPE "Grammar"

using namespace exi;

void Grammar::anchor() {}

static u64 ReadBits(StreamReader& Strm, u32 Bits) {
  u64 Out = 0;
  if (auto E = Strm->readBits64(Out, Bits)) [[unlikely]]
    exi_unreachable("invalid stream read.");
  return Out;
}

GrammarTerm BuiltinGrammar::getTerm(StreamReader& Strm, bool IsStart) {
  auto& Elts = this->getElts(IsStart);
  const u64 Out = ReadBits(Strm, this->getLog(IsStart));
  if EXI_LIKELY(Out < Elts.size())
    return Ok(Elts[Out]);
  return Err(Out - Elts.size());
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

void BuiltinGrammar::dump() const {}
