//===- exicpp/HeapBuffer.hpp ----------------------------------------===//
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
//
//  This file provides a scoped buffer allocated on the heap.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_HEAPBUFFER_HPP
#define EXIP_HEAPBUFFER_HPP

#include "Basic.hpp"
#include <memory>

namespace exi {

class HeapBuffer {
public:
  using BufType  = std::unique_ptr<Char[]>;
public:
  /// Default constructor, no buffer.
  HeapBuffer() = default;
  HeapBuffer(const HeapBuffer&) = delete;
  HeapBuffer(HeapBuffer&&) = default;

  /// Allocates a buffer of size `len`.
  HeapBuffer(std::size_t len) :
   buf(std::make_unique<Char[]>(len)), len(len) {
  }

  /// Allocates a new buffer of size `len`.
  void set(std::size_t len) {
    if EXICPP_LIKELY(len != 0) {
      this->buf = std::make_unique<Char[]>(len);
      this->len = len;
    } else {
      this->reset();
    }
  }

  void reset() {
    buf.reset();
    this->len = 0;
  }

  Char* data() { return buf.get(); }
  const Char* data() const { return buf.get(); }
  std::size_t size() const { return len; }

private:
  BufType buf;
  std::size_t len = 0;
};

} // namespace exi

#endif // EXIP_HEAPBUFFER_HPP
