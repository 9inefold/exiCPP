//===- exi/Grammar/BICoder.cpp --------------------------------------===//
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
/// This file provides a type which can build event maps for builtin encoders.
///
//===----------------------------------------------------------------===//

#include <exi/Grammar/BICoder.hpp>

using namespace exi;
using enum SimpleEventTerm;

static_assert(i32(SE) == 0x0, "Invalid assumption: SE");
static_assert(i32(EE) == 0x1, "Invalid assumption: EE");
static_assert(i32(AT) == 0x2, "Invalid assumption: AT");
static_assert(i32(CH) == 0x3, "Invalid assumption: CH");
static_assert(i32(NS) == 0x4, "Invalid assumption: NS");
static_assert(i32(SD) == 0x5, "Invalid assumption: SD");
static_assert(i32(ED) == 0x6, "Invalid assumption: ED");
static_assert(i32(CM) == 0x7, "Invalid assumption: CM");
static_assert(i32(PI) == 0x8, "Invalid assumption: PI");
static_assert(i32(DT) == 0x9, "Invalid assumption: DT");
static_assert(i32(ER) == 0xA, "Invalid assumption: ER");
static_assert(i32(SC) == 0xB, "Invalid assumption: SC");
