//===- exi/Basic/NBitInt.cpp ----------------------------------------===//
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
/// This file defines a statically sized n-bit integer type.
///
//===----------------------------------------------------------------===//

#include "exi/Basic/NBitInt.hpp"
#include "core/Common/APInt.hpp"

using namespace exi;

APInt NBitIntBase::MakeAPInt(i64 Val, unsigned Bits) {
  // Create signed APInt without truncation.
  return APInt(Bits, static_cast<u64>(Val), true, false);
}

APInt NBitIntBase::MakeAPInt(u64 Val, unsigned Bits) {
  // Create unsigned APInt without truncation.
  return APInt(Bits, Val, false, false);
}
