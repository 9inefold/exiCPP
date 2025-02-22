//===- exi/Basic/FileManager.hpp ------------------------------------===//
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
/// This file defines an interface for dealing with files.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Box.hpp>
#include <core/Common/Fundamental.hpp>
#include <core/Common/IntrusiveRefCntPtr.hpp>
#include <core/Common/StrRef.hpp>

namespace exi {
class MemoryBuffer;

// TODO: Add vfs wrapper

class CachedFile {
	/// The actual buffer containing the input.
	mutable Box<MemoryBuffer> TheBuffer;

public:
	/// Original filename of the cached file.
	StrRef Filename;

	/// If the buffer was a `WritableMemoryBuffer`.
	EXI_PREFER_TYPE(bool)
	unsigned IsMutable : 1;

	/// If the file may change between stat invocations.
	EXI_PREFER_TYPE(bool)
	unsigned IsVolatile : 1;

	/// If the buffer original contents were overridden.
	EXI_PREFER_TYPE(bool)
	unsigned BufferOverridden : 1;

	/// If the buffer content has changed.
	EXI_PREFER_TYPE(bool)
	mutable unsigned IsDirty : 1;
};

class FileManager;

} // namespace exi
