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
#include <core/Common/Box.hpp>
#include <core/Common/EnumTraits.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/Option.hpp>
#include <core/Common/StringMap.hpp>
#include <exi/Basic/Bounded.hpp>

namespace exi {

/// An enum containing all the terminal symbols used for productions.
enum class AlignKind : u8 {
  None            = 0,
  BitPacked       = 1,
  BytePacked      = 2,
  PreCompression  = 3,
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

/// The EXI Options are represented as an EXI Options document, which is an
/// XML document encoded using the EXI format described by the W3C spec.
/// This results in a very compact header format that can be read and written 
/// with very little additional software.
struct ExiOptions {
  /// Alignment of event codes and content items.
  /// Default: BitPacked
  AlignKind Alignment = AlignKind::BitPacked;

  /// EXI compression is used to achieve better compactness.
  /// Default: false
  bool Compression : 1 = false;

  /// Strict interpretation of schemas is used to achieve better compactness.
  /// Default: false
  bool Strict : 1 = false;

  /// Enables self-contained elements.
  /// Default: false
  bool SelfContained : 1 = false;

  /// The set of packed options used by the header.
  struct PreserveOpts {
    bool Comments       : 1 = false; // EventTerm::CM
    bool DTDs           : 1 = false; // EventTerm::{DT, ER}
    bool LexicalValues  : 1 = false;
    bool PIs            : 1 = false; // EventTerm::PI
    bool Prefixes       : 1 = false; // EventTerm::NS
  };

  /// Specifies whether the support for the preservation of comments, pis, etc.
  /// is each enabled.
  /// Default: All false 
  PreserveOpts Preserve = {};

  /// Identify the schema information, if any, used to encode the body.
  /// If the top level is `nullopt` by the time this has reached the processor,
  /// then an error has occurred. It should be explicitly nulled via `xsi:nil`
  /// or communicated out of band.
  /// Default: none
  ///
  /// FIXME: Use a better type once necessary? Or `Option<Option<u64>&>`
  Option<Option<u64>> SchemaID = std::nullopt;

  /// Specify alternate datatype representations for typed values in the body.
  /// When there are no elements in the Options document, no
  /// Datatype Representation Map is used for processing the body. This option
  /// does not take effect when the value of the `Preserve.lexicalValues`
  /// fidelity option is true, or when the EXI stream is a schemaless stream.
  /// Default: none
  ///
  /// FIXME: Use a better type once necessary.
  Box<StringMap</*User Defined Type*/String>> DatatypeRepresentationMap;

  /// Specifies the block size used for EXI compression
  u64 BlockSize = 1'000'000;

  /// Specifies the maximum string length of value content items to be
  /// considered for addition to the string table. 
  /// Default: Unbounded
  Bounded<u64> ValueMaxLength = exi::unbounded;

  /// Specifies the total capacity of value partitions in a string table.
  /// Default: Unbounded
  Bounded<u64> ValuePartitionCapacity = exi::unbounded;

  /// User defined meta-data may be added just before alignment. The data 
  /// MUST NOT be interpreted in a way that alters or extends the EXI format
  /// defined in the spec.
  /// Default: none
  exi::Any UserData;
};

} // namespace exi
