//===- exi/Basic/ExiOptions.hpp -------------------------------------===//
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
/// This file defines the options in the EXI header.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Any.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/EnumTraits.hpp>
#include <exi/Basic/Bounded.hpp>

namespace exi {

/// An enum containing all the terminal symbols used for productions.
enum class AlignKind : u8 {
  None            = 0b00,
  BitPacked       = 0b01,
  BytePacked      = 0b10,
  PreCompression  = 0b11,
};

enum class PreserveKind : u8 {
  Comments        = 0b00001, // EventTerm::CM
  DTDs            = 0b00010, // EventTerm::{DT, ER}
  LexicalValues   = 0b00100,
  PIs             = 0b01000, // EventTerm::PI
  Prefixes        = 0b10000, // EventTerm::NS

  None            = 0b00000,
  Strict          = 0b00100, // Mask for strict mode.
  All             = 0b11111,
};

EXI_MARK_BITWISE_EX(PreserveKind, u8)

class PreserveBuilder {
public:
  using enum PreserveKind;
  using value_type = PreserveKind;
private:
  value_type Opts = None;
public:
  constexpr PreserveBuilder() = default;
  constexpr PreserveBuilder(value_type O) : Opts(O) {}

  constexpr value_type set(value_type O) { return (Opts |= O); }
  constexpr value_type unset(value_type O) { return (Opts &= ~O); }
  constexpr value_type get() const { return Opts; }
  constexpr bool has(value_type O) const { return (Opts & O) != None; }

  constexpr operator PreserveKind() const { return Opts; }
};

//////////////////////////////////////////////////////////////////////////
// Options

class ExiOptions {

  exi::Any UserData;
};

} // namespace exi
