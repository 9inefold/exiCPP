//===- exicpp/Options.hpp -------------------------------------------===//
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

#pragma once

#ifndef EXICPP_OPTIONS_HPP
#define EXICPP_OPTIONS_HPP

#include "Basic.hpp"
#include "Common/MarkBitwise.hpp"
#include "Debug/Format.hpp"

namespace exi {

enum class EnumOpt : unsigned char {
  Compression     = 0b0001,
  Strict          = 0b0010,
  Fragment        = 0b0100,
  SelfContained   = 0b1000,
  /// Used to check values.
  Alignment       = 0b11000000,
};

enum class Align : unsigned char {
  BitPacked       = 0b00000000,
  ByteAlignment   = 0b01000000,
  PreCompression  = 0b10000000,
};

enum class Preserve : unsigned char {
  None          = 0b00000,
  Comments      = 0b00001,
  PIs           = 0b00010,
  DTD           = 0b00100,
  Prefixes      = 0b01000,
  LexicalValues = 0b10000,
  All           = 0b11111,
};

EXICPP_MARK_BITWISE(EnumOpt)
EXICPP_MARK_BITWISE(Align)
EXICPP_MARK_BITWISE(Preserve)

class Options : COptions {
  using BaseType = COptions;
  using Underlying = unsigned char;
public:
  Options() : BaseType() {
    BaseType::enumOpt = 0;
	  BaseType::preserve = 0; // all preserve flags are false by default
	  BaseType::blockSize = 1000000;
	  BaseType::valueMaxLength = INDEX_MAX;
	  BaseType::valuePartitionCapacity = INDEX_MAX;
	  BaseType::user_defined_data = nullptr;
	  BaseType::schemaIDMode = exip::SCHEMA_ID_ABSENT;
	  BaseType::schemaID = {nullptr, 0};
	  BaseType::drMap = nullptr;
  }

  Options& set(EnumOpt O) {
    if EXICPP_UNLIKELY(O == EnumOpt::Alignment) {
      LOG_WARN("Invalid enumOpt 'Alignment'.");
      return *this;
    }
    BaseType::enumOpt |= Underlying(O);
    return *this;
  }

  Options& set(Align A) {
    if (A != Align::BitPacked)
      BaseType::preserve |= Underlying(A);
    else
      BaseType::preserve &= Underlying(~Align::ByteAlignment);
    return *this;
  }

  Options& set(Preserve P) {
    BaseType::preserve |= Underlying(P);
    return *this;
  }

  bool isSet(EnumOpt O) const {
    if (O == EnumOpt::Alignment)
      return anySet(Align::ByteAlignment, Align::PreCompression);
    return Options::HasFlags(getEnumOpt(), O);
  }

  bool isSet(Align A) const {
    const auto V = getAlign();
    if (A == Align::BitPacked) {
      const auto masked = V & Align::ByteAlignment;
      return (masked == Align::BitPacked);
    }
    return Options::HasFlags(V, A);
  }

  bool isSet(Preserve P) const {
    const auto V = getPreserved();
    if (P == Preserve::None)
      // Check if no flags have been set.
      return (V == P);
    return Options::HasFlags(V, P);
  }

  template <typename T, typename U, typename...Args>
  bool isSet(T t, U u, Args...args) const {
    const bool F = isSet(t) && isSet(u);
    return (F && ... && isSet(args));
  }

  template <typename T, typename U, typename...Args>
  bool anySet(T t, U u, Args...args) const {
    const bool F = isSet(t) || isSet(u);
    return (F || ... || isSet(args));
  }

  ////////////////////////////////////////////////////////////////////////

  BaseType getBase() const { return *this; }

  EnumOpt getEnumOpt() const {
    const auto V = BaseType::enumOpt;
    return static_cast<EnumOpt>(V);
  }

  Align getAlign() const {
    const auto V = getEnumOpt() & EnumOpt::Alignment;
    return static_cast<Align>(V);
  }

  Preserve getPreserved() const {
    const auto V = BaseType::preserve;
    return static_cast<Preserve>(V);
  }

private:
  template <typename T>
  ALWAYS_INLINE static bool HasFlags(T V, T flag) {
    static_assert(sizeof(T) == sizeof(Underlying));
    return (V & ~flag) == flag;
  }
};

} // namespace exi

#endif // EXICPP_OPTIONS_HPP
