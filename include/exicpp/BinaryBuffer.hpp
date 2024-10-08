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
#include "Errors.hpp"
#include "Filesystem.hpp"
#include "HeapBuffer.hpp"

namespace exi {

enum class StreamType : int {
  RFile, // A `std::FILE*` reading in.
  WFile, // A `std::FILE*` writing out.
  None,  // Nothing.
  Unknown,
};

enum class BinaryBufferType : int {
  Stack,  // A stack-allocated `Char` buffer.
  Unique, // A `HeapBuffer`.
  Vector, // A `std::vector<Char>`.
  Unknown,
};

template <BinaryBufferType Ty = BinaryBufferType::Unknown>
struct BinaryBuffer;

struct IBinaryBuffer : public CBinaryBuffer {
  template <BinaryBufferType>
  friend struct exi::BinaryBuffer;

  /// @brief Creates a handle to the file at `name`.
  /// @return `true` on failure.
  Error readFile(const fs::path& name);

  /// @brief Creates a handle to the file at `name`.
  /// @return `true` on failure.
  Error writeFile(const fs::path& name);

  ~IBinaryBuffer() { this->destroyStream(); }

protected:
  /// Directly sets the values in the base object.
  void setInternal(Char* data, std::size_t len);
  /// Destroys any stream already set.
  void destroyStream();
private:
  bool isSameBuffer(Char* data, std::size_t len) const {
    return (CBinaryBuffer::buf == data) &&
      (CBinaryBuffer::bufLen == len);
  }

public:
  StreamType stream_type = StreamType::None;
};

//////////////////////////////////////////////////////////////////////////

template <BinaryBufferType Ty>
struct BinaryBuffer : IBinaryBuffer {
  static_assert(int(Ty),
    "Unknown/unimplemented BinaryBuffer type!");
};

using StackBuffer  = BinaryBuffer<BinaryBufferType::Stack>;
using UniqueBuffer = BinaryBuffer<BinaryBufferType::Unique>;
using VecBuffer    = BinaryBuffer<BinaryBufferType::Vector>;

//======================================================================//
// BinaryBuffer
//======================================================================//

template <>
struct BinaryBuffer<BinaryBufferType::Stack> : protected IBinaryBuffer {
  friend class IBinaryBuffer;
  friend class Parser;
  using IBinaryBuffer::readFile;
  using IBinaryBuffer::writeFile;
public:
  constexpr BinaryBuffer() : IBinaryBuffer{} {}

  template <std::size_t N>
  ALWAYS_INLINE BinaryBuffer(Char(&data)[N]) :
   IBinaryBuffer{data, N, 0} {
  }

  template <std::size_t N>
  ALWAYS_INLINE void set(Char(&data)[N]) {
    IBinaryBuffer::setInternal(data, N);
  }

private:
  /// Internal function, used in `IBinaryBuffer::New`.
  constexpr BinaryBuffer(Char* data, std::size_t len) :
   IBinaryBuffer{data, len, 0} {
    if EXICPP_UNLIKELY(!data && len) {
      CBinaryBuffer::bufLen = 0;
    }
  }
};

template <std::size_t N>
struct InlineStackBuffer : public StackBuffer {
  InlineStackBuffer() : StackBuffer(data) {}
private:
  Char data[N] {};
};

template <std::size_t N>
BinaryBuffer(Char(&)[N]) -> StackBuffer;
BinaryBuffer(Char*, std::size_t) -> StackBuffer;

//======================================================================//
// UniqueBuffer
//======================================================================//

template <>
struct BinaryBuffer<BinaryBufferType::Unique> : protected IBinaryBuffer {
  friend class IBinaryBuffer;
  friend class Parser;
public:
  constexpr BinaryBuffer() : IBinaryBuffer{} {}
  ~BinaryBuffer() = default;

  BinaryBuffer(HeapBuffer& buf) :
   IBinaryBuffer{buf.data(), buf.size(), 0} {
  }

  void set(HeapBuffer& buf) {
    this->buf.reset();
    IBinaryBuffer::setInternal(buf.data(), buf.size());
  }

private:
  HeapBuffer::BufType buf = nullptr;
};

BinaryBuffer(HeapBuffer&) -> UniqueBuffer;

} // namespace exi

#endif // EXIP_BINARYBUFFER_HPP
