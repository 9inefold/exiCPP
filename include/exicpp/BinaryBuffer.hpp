//===- exicpp/Content.hpp -------------------------------------------===//
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

#ifndef EXIP_BINARYBUFFER_HPP
#define EXIP_BINARYBUFFER_HPP

#include "Basic.hpp"

namespace exi {

enum class BinaryBufferType {
  Stack,
  Vector,
  Unknown,
};

template <BinaryBufferType>
struct BinaryBuffer;

template <>
struct BinaryBuffer<BinaryBufferType::Stack> : protected CBinaryBuffer {
  friend class Parser;
  constexpr BinaryBuffer() = default;

  template <std::size_t N>
  ALWAYS_INLINE BinaryBuffer(Char(&data)[N]) :
   CBinaryBuffer{data, N, 0} {}

  template <std::size_t N>
  ALWAYS_INLINE void set(Char(&data)[N]) {
    return this->setInternal(data, N);
  }

private:
  void setInternal(Char* data, std::size_t len) {
    CBinaryBuffer::buf = data;
    CBinaryBuffer::bufLen = len;
    CBinaryBuffer::bufContent = 0;
  }
};

using StackBuffer = BinaryBuffer<BinaryBufferType::Stack>;

template <std::size_t N>
BinaryBuffer(Char(&)[N]) -> StackBuffer;

} // namespace exi

#endif // EXIP_BINARYBUFFER_HPP
