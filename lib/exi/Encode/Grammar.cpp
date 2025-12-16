//===- exi/Encode/Grammar.cpp ---------------------------------------===//
//
// Copyright (C) 2024 Ninefold
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

#include <exi/Encode/Grammar.hpp>
#include <core/Support/WithColor.hpp>
#include <exi/Encode/StringTable.hpp>

#define DEBUG_TYPE "Grammar"

using namespace exi;
using namespace exi::encode;

void BuiltinGrammar::LogDiscarded(LocalNameInfo* LN) {
  WithColor(dbgs(), raw_ostream::BRIGHT_YELLOW)
    << "WARNING: Discarded " << LN->name() << '\n';
}
