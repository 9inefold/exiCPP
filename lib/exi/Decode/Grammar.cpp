//===- exi/Decode/Grammar.cpp ---------------------------------------===//
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

#include <exi/Decode/Grammar.hpp>
#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/CompactID.hpp>
#include <Grammar/Decode/SchemaGet.hpp>

#define DEBUG_TYPE "Grammar"

using namespace exi;
using namespace exi::decode;

void BuiltinGrammar::setLog(bool IsStart) {
  if (IsStart) {
    const u32 Log = getStartTagCount();
    StartTagLog = ID_Log2(Log);
  } else {
    const u32 Log = getElementCount();
    ElementLog = ID_Log2(Log);
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
