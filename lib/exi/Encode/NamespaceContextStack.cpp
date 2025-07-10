//===- exi/Encode/NamespaceContextStack.cpp --------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file implements a stack of scopes used to save and restore namespace
/// contexts when nesting.
///
//===----------------------------------------------------------------===//

#include <exi/Encode/NamespaceContextStack.hpp>
#include <core/Support/Logging.hpp>
#include <exi/Basic/Except.hpp>
#include <exi/Encode/StringTable.hpp>

using namespace exi;
using encode::StringTable;

// TODO: Profile PRESERVE_MOST

EXI_COLD EXI_PRESERVE_MOST void NSContextStack::pushScope(
 StringTable& SM, ArrayRef<NSContextStack::value_type> Elts) {
  exi_invariant(!Elts.empty());
  const usize N = Elts.size();
  Scopes.reserve_back(N + 1);
#if 0
  exi_todo("Implement parts in StringTable for pushScope...");
  for (auto* Entry : Elts) {
    if EXI_NEVER(Entry == nullptr)
      continue;
  }
#endif
  const usize OldSize = Scopes.size();
  Scopes.resize_for_overwrite(OldSize + N);
  std::memcpy(Scopes.data() + OldSize, Elts.data(), Elts.size_in_bytes());
  this->Head = addHeadImpl(N);
}

EXI_COLD EXI_PRESERVE_MOST void NSContextStack::popScope(StringTable& SM) {
  if EXI_NEVER(Head->isTail())
    Throw<runtime_error>("Attempted to pop() from the tail scope!");
  
  exi_invariant(Head->Depth == 0);
  for (auto* Entry : Head->arr())
    SM.exitNamespace(Entry);

  const auto N = Head->size();
  Scopes.pop_back_n(N + 1);
  this->Head = getHead();
}
