//===- Support/SmallVecMemoryBuffer.hpp ------------------------------===//
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
// This file declares a wrapper class to hold the memory into which an
// object will be generated.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/SmallVec.hpp>
#include <Support/MemoryBuffer.hpp>
#include <Support/raw_ostream.hpp>

namespace exi {

/// SmallVec-backed MemoryBuffer instance.
///
/// This class enables efficient construction of MemoryBuffers from SmallVec
/// instances. This is useful for MCJIT and Orc, where object files are streamed
/// into SmallVecs, then inspected using ObjectFile (which takes a
/// MemoryBuffer).
class SmallVecMemoryBuffer : public MemoryBuffer {
public:
  /// Construct a SmallVecMemoryBuffer from the given SmallVec r-value.
  SmallVecMemoryBuffer(SmallVecImpl<char> &&SV,
                       bool RequiresNullTerminator = true)
      : SmallVecMemoryBuffer(std::move(SV), "<in-memory object>",
                                RequiresNullTerminator) {}

  /// Construct a named SmallVecMemoryBuffer from the given SmallVec
  /// r-value and StrRef.
  SmallVecMemoryBuffer(SmallVecImpl<char> &&SV, StrRef Name,
                       bool RequiresNullTerminator = true)
      : SV(std::move(SV)), BufferName(String(Name)) {
    if (RequiresNullTerminator) {
      this->SV.push_back('\0');
      this->SV.pop_back();
    }
    init(this->SV.begin(), this->SV.end(), false);
  }

  // Key function.
  ~SmallVecMemoryBuffer() override;

  StrRef getBufferIdentifier() const override { return BufferName; }

  BufferKind getBufferKind() const override { return MemoryBuffer_Malloc; }

private:
  SmallVec<char, 0> SV;
  String BufferName;
};

} // namespace exi
