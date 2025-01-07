//===- Support/SwapByteOrder.hpp ------------------------------------===//
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
//
// This file declares generic and optimized functions to swap the byte order of
// an integral type.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/EnumTraits.hpp>
#include <Common/bit.hpp>
#include <cstdint>
#include <type_traits>

namespace exi::sys {

EXI_CONST bool IsBigEndianHost =
  exi::endianness::native == exi::endianness::big;

static const bool IsLittleEndianHost = !IsBigEndianHost;

inline unsigned char      getSwappedBytes(unsigned char      C) { return exi::byteswap(C); }
inline   signed char      getSwappedBytes( signed  char      C) { return exi::byteswap(C); }
inline          char      getSwappedBytes(         char      C) { return exi::byteswap(C); }

inline unsigned short     getSwappedBytes(unsigned short     C) { return exi::byteswap(C); }
inline   signed short     getSwappedBytes(  signed short     C) { return exi::byteswap(C); }

inline unsigned int       getSwappedBytes(unsigned int       C) { return exi::byteswap(C); }
inline   signed int       getSwappedBytes(  signed int       C) { return exi::byteswap(C); }

inline unsigned long      getSwappedBytes(unsigned long      C) { return exi::byteswap(C); }
inline   signed long      getSwappedBytes(  signed long      C) { return exi::byteswap(C); }

inline unsigned long long getSwappedBytes(unsigned long long C) { return exi::byteswap(C); }
inline   signed long long getSwappedBytes(  signed long long C) { return exi::byteswap(C); }

inline float getSwappedBytes(float C) {
  return exi::bit_cast<float>(exi::byteswap(exi::bit_cast<u32>(C)));
}

inline double getSwappedBytes(double C) {
  return exi::bit_cast<double>(exi::byteswap(exi::bit_cast<u64>(C)));
}

template <typename T>
inline std::enable_if_t<std::is_enum_v<T>, T> getSwappedBytes(T C) {
  return static_cast<T>(exi::byteswap(exi::to_underlying(C)));
}

template<typename T>
inline void swapByteOrder(T& Value) {
  Value = getSwappedBytes(Value);
}

} // namespace exi::sys
