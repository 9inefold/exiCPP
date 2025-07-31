//===- exi/Grammar/BITypes.hpp --------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file defines types for builtin schemas.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Array.hpp>
#include <core/Common/EnumArray.hpp>
#include <exi/Grammar/BIState.hpp>

namespace exi {

/// Small EventCode for use in `BIInfo`.
struct EXI_TRIVIAL_ABI SEventCode {
  Array<u8, 3> Data = {};  // [x.y.z]
  Array<u8, 3> Bits = {};  // [[x].[y].[z]]
  i8 Length = 0;           // Number of pieces.
};

struct EXI_TRIVIAL_ABI BIInfo {
  u8 Offset = 0;
  SEventCode Code = {};
};

using BIInfoArray = EnumeratedArray<BIInfo, BIGrammarState,
  BIGrammarState::Last, BIGrammarState::DocContent>;

} // namespace exi
