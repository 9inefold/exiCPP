//===- Support/FmtBuffer.cpp ----------------------------------------===//
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


#include <Support/FmtBuffer.hpp>
#include <Support/MathExtras.hpp>
#include <Support/raw_ostream.hpp>
#include <cstring>
#include <fmt/format.h>

using namespace exi;
using WriteState = FmtBuffer::WriteState;

////////////////////////////////////////////////////////////////////////
// Ctors

FmtBuffer::FmtBuffer() = default;

FmtBuffer::FmtBuffer(char* data, usize cap) :
 Data(data), Size(0), Cap(IntCast<size_type>(cap)) {
  exi_assert(data || cap == 0, "Invalid buffer size.");
}

FmtBuffer::FmtBuffer(MutArrayRef<char> A) :
 FmtBuffer(A.data(), A.size()) {
}

FmtBuffer::FmtBuffer(char* data, usize size, usize cap) : FmtBuffer(data, cap) {
  exi_invariant(size <= Cap, "size is out of range.");
  this->Size = IntCastOrZero<size_type>(size);
}

////////////////////////////////////////////////////////////////////////
// Methods

WriteState FmtBuffer::write(StrRef Str) {
  if EXI_UNLIKELY(!Data)
    return NoWrite;
  else if (Str.empty())
    return FullWrite;
  
  auto [ptr, rcap] = this->getPtrAndRCap();
  const usize writeCount = std::min(rcap, Str.size());
  std::memcpy(ptr, Str.data(), writeCount);

  this->Size += IntCast<size_type>(writeCount);
  return (Str.size() <= rcap) ? FullWrite : PartialWrite;
}

WriteState FmtBuffer::formatImpl(fmt::string_view Str, fmt::format_args args) {
  if EXI_UNLIKELY(!Data)
    return NoWrite;

  auto [ptr, rcap] = this->getPtrAndRCap();
  const auto result = fmt::vformat_to_n(ptr, rcap, Str, args);
  if (rcap == 0) {
    return (result.size == 0)
      ? FullWrite : NoWrite;
  }

  const auto newSize = IntCast<size_type>(Size + result.size);
  const bool isFullWrite = (newSize <= Cap);

  this->Size = std::min(newSize, Cap);
  return isFullWrite ? FullWrite : PartialWrite;
}

WriteState FmtBuffer::setLast(char C) {
  if (!Data || !Cap)
    return NoWrite;
  
  if (!this->isFull()) {
    this->Data[Size] = C;
    ++this->Size;
    return FullWrite;
  }

  // Full, so just write the last character.
  exi_invariant(Size > 0, "Invalid index.");
  this->Data[Size - 1] = C;
  return PartialWrite;
}

std::pair<char*, usize> FmtBuffer::getPtrAndRCap() const {
  const size_type rcap = (Cap - Size);
  return {Data + Size, static_cast<usize>(rcap)};
}

void FmtBuffer::zeroBuffer() const {
  exi_invariant(Size <= Cap, "size is out of range.");
  if EXI_LIKELY(Data && Size)
    std::memset(Data, 0, Size);
}

raw_ostream& exi::operator<<(raw_ostream& OS, const FmtBuffer& buf) {
  return OS.write(buf.Data, buf.Size);
}
