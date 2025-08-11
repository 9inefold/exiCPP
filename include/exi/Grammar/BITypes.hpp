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

template <typename T>
using BIGrammarStateArray = EnumeratedArray<BIInfo, BIGrammarState,
  BIGrammarState::Last, BIGrammarState::DocContent>;

/// Stores the data for builtin decoder schemas.
using BIInfoArray = BIGrammarStateArray<BIInfo>;

/// EventCode which represents a `([x.y.z], Size)`.
struct FullEventCode {
  u32 Data : 24 = 0;
  u32 Bits : 8  = 0;
};

/// Partial EventCode which represents a `(x.[y.z], Size)`.
struct SecondLevelEventCode {
  u32 Data : 16 = 0;
  u32 Bits : 16 = 0;
};

} // namespace exi
