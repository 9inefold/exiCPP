//===- exi/Basic/FileEntry.cpp --------------------------------------===//
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
/// This file implements FileEntry functions.
///
//===----------------------------------------------------------------===//

#include <exi/Basic/FileEntry.hpp>
#include <core/Support/MemoryBuffer.hpp>
#include <core/Support/VirtualFilesystem.hpp>

using namespace exi;

FileEntry::FileEntry() : UniqueID(0, 0) {}

FileEntry::~FileEntry() = default;

void FileEntry::closeFile() const { TheFile.release(); }

Option<MemoryBufferRef> FileEntry::getBufferIfLoaded() const {
	if (TheBuffer)
		return TheBuffer->getMemBufferRef();
	return std::nullopt;
}

WritableMemoryBuffer* FileEntry::getMutBuffer() const {
	exi_assert(TheBuffer && IsMutable);
	auto* Buf = dynamic_cast<WritableMemoryBuffer*>(&*TheBuffer);
	return EXI_LIKELY(Buf) ? Buf : nullptr;
}

Option<WritableMemoryBuffer&> FileEntry::getWriteBufferIfLoaded() const {
	if (!TheBuffer || !IsMutable)
		return std::nullopt;
	if (auto* Buf = this->getMutBuffer())
		return *Buf;
	return std::nullopt;
}

void FileEntry::setBuffer(Box<MemoryBuffer> Buffer) {
	IsMutable = false;
	TheBuffer = std::move(Buffer);
}

void FileEntry::setBuffer(Box<WritableMemoryBuffer> Buffer) {
	IsMutable = true;
	TheBuffer = std::move(Buffer);
}
