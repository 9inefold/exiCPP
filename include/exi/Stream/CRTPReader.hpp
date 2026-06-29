//===- exi/Stream/CRTPReader.hpp ------------------------------------===//
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
/// This file defines functions for in-order readers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <exi/Stream/Reader.hpp>
#include <core/Common/CRTPTraits.hpp>

// HACK: Remove ReaderMethods if this is bumped to C++23

/// Defines the `using`s specifically for the `CRTPReader`.
#define EXI_CRTPREADER_USING_ONLY(DERIVED)  \
  using CRTPReader<DERIVED>::readBit;       \
  using CRTPReader<DERIVED>::readByte;      \
  using CRTPReader<DERIVED>::readBits64;    \
  using CRTPReader<DERIVED>::readUInt;      \
  using CRTPReader<DERIVED>::readBits;
/// Defines all the `using`s required for the `CRTPReader`.
#define EXI_CRTPREADER_USING(DERIVED)       \
  using ReaderBase::readBit;                \
  using ReaderBase::readByte;               \
  using ReaderBase::readBits64;             \
  using ReaderBase::readUInt;               \
  EXI_CRTPREADER_USING_ONLY(DERIVED)

namespace exi {

/// Defines functions that can be inherited
template <class Derived> class CRTPReader {
  EXI_CRTP_DEFINE_SUPER(Derived)

  template <typename T>
  ALWAYS_INLINE static ExiError SetData(T& Out, const ExiResult<T>& R) {
    if EXI_LIKELY(R.is_ok()) {
      Out = *R;
      return ExiError::OK;
    } else {
      Out = 0;
      return R.error();
    }
  }

public:
  /// Reads a single bit.
  ExiError readBit(bool& Out) {
    const auto R = super()->Derived::readBit();
    return CRTPReader::SetData(Out, R);
  }

  ExiError readByte(u8& Out) {
    const auto R = super()->Derived::readByte();
    return CRTPReader::SetData(Out, R);
  }

  /// Reads a variable number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  ExiError readBits64(u64& Out, StreamBase::size_type Bits) {
    const auto R = super()->Derived::readBits64(Bits);
    return CRTPReader::SetData(Out, R);
  }

  /// Reads an `Unsigned Integer` with a maximum of 8 octets.
  /// See https://www.w3.org/TR/exi/#encodingUnsignedInteger.
  ExiError readUInt(u64& Out) {
    const auto R = super()->Derived::readUInt();
    return CRTPReader::SetData(Out, R);
  }

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  template <unsigned Bits>
  ExiError readBits(ubit<Bits>& Out) {
    const auto R = super()->Derived::readBits64(Bits);
    Out = ubit<Bits>::FromBits(R.value_or(0));
    return R.error_or(ExiError::OK);
  }

  /// Reads a static number of bits (max of 64).
  /// This means data is peeked, then the position is advanced.
  /// @attention This function ignores errors.
  template <unsigned Bits>
  ExiResult<ubit<Bits>> readBits() {
    auto Data = super()->Derived::readBits64(Bits);
    if EXI_UNLIKELY(Data.is_err())
      return Err(Data.error());
    // TODO: Mask data?
    return ubit<Bits>::FromBits(*Data);
  }
};

} // namespace exi
