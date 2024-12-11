//===- Support/FmtBuffer.hpp ----------------------------------------===//
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

#include <Common/ArrayRef.hpp>
#include <Common/Fundamental.hpp>
#include <Common/String.hpp>
#include <Common/StringExtras.hpp>
#include <Support/MathExtras.hpp>
#include <fmt/base.h>

namespace exi {

class raw_ostream;

template <usize N>
class StaticFmtBuffer;

namespace H {
template <typename T>
concept has_toCommonStringBuf = requires (T t) {
  toCommonStringBuf(FWD(t));
};
} // namespace H

class EXI_GSL_POINTER FmtBuffer {
public:
  using value_type = char_t;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using size_type = u32;
  using difference_type = ptrdiff;

  enum WriteState {
    NoWrite       = 0,
    PartialWrite  = 1,
    FullWrite     = 2,
  };

public:
  constexpr FmtBuffer() = default;

  EXI_INLINE constexpr FmtBuffer(FmtBuffer&& RHS) :
   FmtBuffer(RHS.Data, RHS.Size, RHS.Cap) {
    RHS.clear();
  }

  FmtBuffer& operator=(FmtBuffer&& RHS) {
    *this = RHS;
    RHS.clear();
    return *this;
  }

  constexpr FmtBuffer(char* data, usize cap) :
   Data(data), Size(0), Cap(IntCast<size_type>(cap)) {
    exi_assert(data || cap == 0, "Invalid buffer size.");
  }

  FmtBuffer(MutArrayRef<char> A) :
   FmtBuffer(A.data(), A.size()) {
  }

  template <H::is_char_kind Ch>
  FmtBuffer(Ch* beg, Ch* end) :
   FmtBuffer(toCommonStringBuf(beg, (end - beg))) {
    exi_invariant(beg <= end, "Invalid range.");
  }

  template <typename T>
  requires H::has_toCommonStringBuf<T>
  FmtBuffer(T&& Buffer) : FmtBuffer(toCommonStringBuf(Buffer)) {}

private:
  EXI_INLINE constexpr FmtBuffer(const FmtBuffer& RHS) :
   FmtBuffer(RHS.Data, RHS.Size, RHS.Cap) {
  }

  FmtBuffer& operator=(const FmtBuffer&) = default;

  constexpr FmtBuffer(char* data, usize size, usize cap) :
   FmtBuffer(data, cap) {
    exi_invariant(size <= Cap, "size is out of range.");
    this->Size = IntCastOrZero<size_type>(size);
  }

public:
  template <typename...Args>
  WriteState format(fmt::format_string<Args...> S, Args&&...args) {
    return this->formatImpl(S.str, fmt::vargs<Args...>{{args...}});
  }

  /// Writes a simple StrRef.
  WriteState write(StrRef S);

  /// Writes one if not full, otherwise sets the last character.
  WriteState setLast(char C);

  ////////////////////////////////////////////////////////////////////////
  // Buffer Modification

  /// Reinitializes from `buffer`.
  void reinit(FmtBuffer&& buffer) {
    *this = std::move(buffer);
  }

  /// Zeros the buffer then reinitializes.
  void reinitAndZero(FmtBuffer&& buffer) {
    this->zeroBuffer();
    this->reinit(std::move(buffer));
  }

  /// Sets the size back to zero.
  void reset() { this->Size = 0; }

  /// Zeros the buffer and resets.
  void resetAndZero() {
    this->zeroBuffer();
    this->reset();
  }

  /// Resets all data.
  void clear() {
    this->clear();
    this->Data = nullptr;
    this->Cap  = 0;
  }

  /// Zeros the buffer and clears.
  void clearAndZero() {
    this->zeroBuffer();
    this->clear();
  }

  ////////////////////////////////////////////////////////////////////////
  // Transformation

  StrRef str() const {
    return StrRef(Data, Size);
  }

  template <H::is_char_kind Ch = char>
  MutArrayRef<Ch> arr() {
    return MutArrayRef<Ch>(reinterpret_cast<Ch*>(Data), Size);
  }

  template <H::is_char_kind Ch = char>
  ArrayRef<Ch> arr() const { return this->carr<Ch>(); }

  template <H::is_char_kind Ch = char>
  ArrayRef<Ch> carr() const {
    return ArrayRef<Ch>(reinterpret_cast<const Ch*>(Data), Size);
  }

  ////////////////////////////////////////////////////////////////////////
  // Observers

  bool empty() const { return Size == 0; }
  bool isEmpty() const { return this->empty(); }

  bool full() const { return Size == Cap; }
  bool isFull() const { return this->full(); }

  const_pointer data() const { return Data; }
  usize size() const { return Size; }
  usize capacity() const { return Cap; }

  iterator begin() const { return data(); }
  iterator end()   const { return data() + Size; }

  friend raw_ostream& operator<<(raw_ostream& OS, const FmtBuffer& buf);

protected:
  /// Handles variadic formatting.
  WriteState formatImpl(fmt::string_view S, fmt::format_args args);

  /// Returns a pair of `[Buffer Offset, Remaining Capacity]`.
  std::pair<char*, usize> getPtrAndRCap() const {
    const size_type rcap = (Cap - Size);
    return {Data + Size, static_cast<usize>(rcap)};
  }

  void zeroBuffer() const;

private:
  char* Data = nullptr;
  size_type Size = 0u, Cap = 0u;
};

template <usize N>
class StaticFmtBuffer : public FmtBuffer {
  using FmtBuffer::FmtBuffer;
  using FmtBuffer::reinit;
  using FmtBuffer::reinitAndZero;
  using FmtBuffer::clear;
  using FmtBuffer::clearAndZero;
public:
  StaticFmtBuffer() : FmtBuffer(Buffer, N) {}
  StaticFmtBuffer(const StaticFmtBuffer&) = delete;
  StaticFmtBuffer(StaticFmtBuffer&&) = delete;

private:
  char Buffer[N ? N : 1];
};

// Is a friend to `FmtBuffer`.
raw_ostream& operator<<(raw_ostream& OS, const FmtBuffer& buf);

} // namespace exi
