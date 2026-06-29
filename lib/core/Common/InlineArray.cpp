//===- Common/InlineArray.cpp ---------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements an array with the size inline.
///
//===----------------------------------------------------------------===//

#include <Common/InlineArray.hpp>
#include <Common/InlineStr.hpp>

using namespace exi;

namespace exi {
template struct InlineArray<char, u16>;
template struct InlineArrayCommon<InlineStr, char>;
} // namespace exi

EXI_FLATTEN InlineStr* exi::make_inlinestr(StrRef S, bool NullTerminate) {
  MallocAllocator Alloc {};
  return make_inlinestr(Alloc, S, NullTerminate);
}
