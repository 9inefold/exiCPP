//===- Buf.hpp ------------------------------------------------------===//
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
/// This file provides some utility buffers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <ArrayRef.hpp>
#include <Fundamental.hpp>

namespace re {

struct AnsiString;
struct UnicodeString;

inline constexpr usize kMaxPath = 260;

namespace H {

template <typename Ch> struct GenericBuf {
  using Char = Ch;
  static constexpr usize kGBufSize = kMaxPath + 1;
public:
  constexpr Char* data() { return Data; }
  constexpr const Char* data() const { return Data; }
  constexpr usize size() const { return Size; }
  constexpr usize sizeInBytes() const { return Size * sizeof(Char); }
  static constexpr usize capacity() { return kGBufSize - 1; }
  static constexpr usize capacityInBytes() { return capacity() * sizeof(Char); }
  constexpr bool empty() const { return Size == 0 || *Data == Ch(0); }
public:
  usize Size = 0;
  Char Data[kGBufSize] {};
};

extern template struct GenericBuf<char>;
extern template struct GenericBuf<wchar_t>;

} // namespace H

struct NameBuf : public H::GenericBuf<char> {
  using Base = H::GenericBuf<Char>;
public:
  void loadNt(AnsiString Str);
  void loadNt_U(UnicodeString UStr);
  void setNt(AnsiString& Str) const;

  MutArrayRef<Char> buf();
  ArrayRef<Char> buf() const;
};

struct WNameBuf : public H::GenericBuf<wchar_t> {
  using Base = H::GenericBuf<Char>;
public:
  void loadNt(UnicodeString UStr);
  void setNt(UnicodeString& UStr) const;

  MutArrayRef<Char> buf();
  ArrayRef<Char> buf() const;
};

} // namespace re
