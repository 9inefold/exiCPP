//===- Support/MemoryBufferRef.hpp -----------------------------------===//
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
//  This file defines the MemoryBuffer interface.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/StrRef.hpp>

namespace exi {

class MemoryBuffer;
class WritableMemoryBuffer;

namespace H {

template <typename MB>
struct MemoryBufferClassImpl {
  COMPILE_FAILURE(MemoryBufferClassImpl,
    "Invalid MemoryBufferClass, must be [Writable]MemoryBuffer!");
};

template <>
struct MemoryBufferClassImpl<MemoryBuffer> {
  static constexpr bool is_mutable = false;
};

template <>
struct MemoryBufferClassImpl<WritableMemoryBuffer> {
  static constexpr bool is_mutable = true;
};

} // namespace H

class MemoryBufferRef {
  StrRef Buffer;
  StrRef Identifier;

public:
  MemoryBufferRef() = default;
  MemoryBufferRef(const MemoryBuffer &Buffer);
  MemoryBufferRef(StrRef Buffer, StrRef Identifier)
      : Buffer(Buffer), Identifier(Identifier) {}

  StrRef getBuffer() const { return Buffer; }
  StrRef getBufferIdentifier() const { return Identifier; }

  const char *getBufferStart() const { return Buffer.begin(); }
  const char *getBufferEnd() const { return Buffer.end(); }
  size_t getBufferSize() const { return Buffer.size(); }

  /// Check pointer identity (not value) of identifier and data.
  friend bool operator==(const MemoryBufferRef &LHS,
                         const MemoryBufferRef &RHS) {
    return LHS.Buffer.begin() == RHS.Buffer.begin() &&
           LHS.Buffer.end() == RHS.Buffer.end() &&
           LHS.Identifier.begin() == RHS.Identifier.begin() &&
           LHS.Identifier.end() == RHS.Identifier.end();
  }

  friend bool operator!=(const MemoryBufferRef &LHS,
                         const MemoryBufferRef &RHS) {
    return !(LHS == RHS);
  }
};

template <typename MB>
inline constexpr bool kMemoryBufferMutability
    = H::MemoryBufferClassImpl<MB>::is_mutable;

} // namespace exi
