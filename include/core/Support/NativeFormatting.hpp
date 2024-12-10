//===- Support/NativeFormatting.hpp ---------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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

#pragma once

#include <Common/Fundamental.hpp>
#include <Common/Option.hpp>

namespace exi {

class raw_ostream;

enum class FloatStyle { Exponent, ExponentUpper, Fixed, Percent };
enum class IntStyle { Integer, Number };
enum class HexPrintStyle { Upper, Lower, PrefixUpper, PrefixLower };

usize getDefaultPrecision(FloatStyle Style);

bool isPrefixedHexStyle(HexPrintStyle S);

void write_integer(raw_ostream &S, unsigned int N, usize MinDigits,
                   IntStyle Style);
void write_integer(raw_ostream &S, int N, usize MinDigits, IntStyle Style);
void write_integer(raw_ostream &S, unsigned long N, usize MinDigits,
                   IntStyle Style);
void write_integer(raw_ostream &S, long N, usize MinDigits,
                   IntStyle Style);
void write_integer(raw_ostream &S, unsigned long long N, usize MinDigits,
                   IntStyle Style);
void write_integer(raw_ostream &S, long long N, usize MinDigits,
                   IntStyle Style);

void write_hex(raw_ostream &S, u64 N, HexPrintStyle Style,
               Option<usize> Width = std::nullopt);
void write_double(raw_ostream &S, double D, FloatStyle Style,
                  Option<usize> Precision = std::nullopt);

} // namespace exi
