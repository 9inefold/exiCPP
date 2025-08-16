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

#include <core/Support/raw_ostream.hpp>
#include <exi/Basic/Except.hpp>

namespace exi {

void ReaderBase::anchor() {}
void WriterBase::anchor() {}

void OrderedReader::anchor() {}
void BitReader::anchor() {}
void ByteReader::anchor() {}

void OrderedWriter::anchor() {}
void BitWriter::anchor() {}
void ByteWriter::anchor() {}

#if EXI_HAS_CHANNEL_READER
void ChannelReader::anchor() {}
void BlockReader::anchor() {}
void DeflateReader::anchor() {}
#endif

//////////////////////////////////////////////////////////////////////////
// Miscellaneous

[[noreturn]] EXI_SLOW_PATH void
 OrderedReader::WriteError(const ExiError& E, const char* Msg) {
  dbgs() << E << '\n';
  ThrowDyn<runtime_error>(Msg);
}

#if EXI_DEBUG || EXI_ENABLE_DUMP
EXI_DUMP_METHOD void OrderedWriter::dump() const {
  this->dumpWord();
  this->dumpData();
}

EXI_DUMP_METHOD void OrderedWriter::dumpData() const {
  if (Buffer.empty())
    return;
  errs() << "Data[" << Buffer.size() << "]:";
  for (usize Ix = 0; Ix < Buffer.size(); ++Ix) {
    if (Ix % 8 == 0)
      errs() << "\n  ";
    errs() << format("{:08b} ", u8(Buffer[Ix]));
  }
  errs() << '\n';
}

static void DumpWordImpl(const StreamBase::word_t Word, usize NBits) {
  unsigned Bits = NBits % 8;
  unsigned FullBytes = NBits / 8;
  unsigned Ix = 1;
  for (; Ix <= FullBytes; ++Ix) {
    unsigned At = 8 - Ix;
    StreamBase::word_t V = Word >> (At * 8);
    errs() << format("{:08b} ", (V & 0xFF));
  }

  if (Bits) {
    unsigned At = 8 - Ix;
    StreamBase::word_t V = Word >> ((At * 8) + (8 - Bits));
    V &= StreamBase::MakeNBitMask(Bits);
    errs() << format("{:0{}b} ", V, Bits);
  }

  errs() << '\n';
}

EXI_DUMP_METHOD void OrderedWriter::dumpWord() const {
  if (!BitsInStore)
    return;
  errs() << "Word[@" << BitsInStore << "]:\n  ";
  DumpWordImpl(Store, BitsInStore);
}
#endif

} // namespace exi
