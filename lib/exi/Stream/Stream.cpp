//===- exi/Stream/Stream.cpp ----------------------------------------===//
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
/// This file defines anchors for readers and writers.
///
//===----------------------------------------------------------------===//

#include <exi/Stream/Reader.hpp>
#include <exi/Stream/OrderedReader.hpp>
#include <exi/Stream/ChannelReader.hpp>

#include <exi/Stream/Writer.hpp>
#include <exi/Stream/OrderedWriter.hpp>
// #include <exi/Stream/ChannelWriter.hpp>

namespace exi {

void ReaderBase::anchor() {}
void WriterBase::anchor() {}

void OrderedReader::anchor() {}
void BitReader::anchor() {}
void ByteReader::anchor() {}

void OrderedWriter::anchorX() {}
void BitWriter::anchor() {}
void ByteWriter::anchor() {}

#if EXI_HAS_CHANNEL_READER
void ChannelReader::anchor() {}
void BlockReader::anchor() {}
void DeflateReader::anchor() {}
#endif

} // namespace exi
