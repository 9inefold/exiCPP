//===- exi/Decode/UnifyBuffer.cpp -----------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file provides a unified interface for passing buffers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/ArrayRef.hpp>
#include <core/Common/StringExtras.hpp>
#include <core/Support/MemoryBufferRef.hpp>

namespace exi {

/// A proxy type for simpler buffer interfaces. Instead of requiring overloads
/// for buffer-like types (ArrayRef, StrRef, MemoryBufferRef, etc.), you can
/// accept this, and it will unify them.
class UnifiedBuffer {
	ArrayRef<u8> Data;
	StrRef Name = "Unknown buffer";

public:
	UnifiedBuffer() = default;
  UnifiedBuffer(ArrayRef<u8> Buffer) : Data(Buffer) {}
  UnifiedBuffer(StrRef Buffer) : Data(arrayRefFromStringRef(Buffer)) {}
  UnifiedBuffer(MemoryBufferRef MB) :
	 Data(arrayRefFromStringRef(MB.getBuffer())),
	 Name(MB.getBufferIdentifier()) {
	}

	/// Gets buffer as an `ArrayRef`.
	ArrayRef<u8> arr() const { return Data; }
	/// Gets buffer as a `StrRef`.
	StrRef str() const { return toStringRef(Data); }
	/// Gets buffer as a `MemoryBufferRef`.
	MemoryBufferRef buf() const {
		return MemoryBufferRef(toStringRef(Data), Name);
	}

	operator ArrayRef<u8>() const { return this->arr(); }
	explicit operator StrRef() const { return this->str(); }
	explicit operator MemoryBufferRef() const { return this->buf(); }
};

} // namespace exi
