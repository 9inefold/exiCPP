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
#include "SchemaGet.hpp"

#define DEBUG_TYPE "Grammar"

using namespace exi;

void Grammar::anchor() {}

GrammarTerm BuiltinGrammar::getTerm(ExiDecoder& Decoder) {
  return Err(0);
}

void BuiltinGrammar::dump() const {}
