//===- Buf.cpp ------------------------------------------------------===//
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

#include <Buf.hpp>
#include <Strings.hpp>
#include "NtImports.hpp"

using namespace re;

template struct re::H::GenericBuf<char>;
template struct re::H::GenericBuf<wchar_t>;

template <class StrType, class BufType>
static inline void DoSet(StrType& Out, const BufType& In) {
  using Char = typename BufType::Char;
  Out.Length = In.sizeInBytes();
  Out.MaximumLength = In.capacityInBytes();
  Out.Buffer = const_cast<Char*>(In.data());
}

template <class StrType, class BufType>
static inline void DoLoad(const StrType& In, BufType& Out) {
  using Char = typename BufType::Char;
  if UNLIKELY(In.Length > Out.capacityInBytes()) {
    re_assert(In.Length <= Out.capacityInBytes());
    Out.Size = 0;
    Out.Data[0] = '\0';
    return;
  }

  const usize Len = (In.Length / sizeof(Char));
  if (In.Buffer != Out.data())
    // Only copy if this isn't a reference to this buffer.
    (void) Memcpy(Out.Data, In.Buffer, Len);
  Out.Size = Len;
  Out.Data[Len] = '\0';
}

//////////////////////////////////////////////////////////////////////////
// NameBuf

void NameBuf::loadNt(AnsiString Str) {
  DoLoad(Str, *this);
}

void NameBuf::loadNt_U(UnicodeString UStr) {
  AnsiString Str;
  this->setNt(Str);
  RtlUnicodeStringToAnsiString(&Str, &UStr, false);
  this->loadNt(Str);
}

void NameBuf::setNt(AnsiString& Str) const {
  DoSet(Str, *this);
}

MutArrayRef<char> NameBuf::buf() {
  return {data(), size()};
}

ArrayRef<char> NameBuf::buf() const {
  return {data(), size()};
}

//////////////////////////////////////////////////////////////////////////
// WNameBuf

void WNameBuf::loadNt(UnicodeString UStr) {
  DoLoad(UStr, *this);
}

void WNameBuf::setNt(UnicodeString& UStr) const {
  DoSet(UStr, *this);
}

MutArrayRef<wchar_t> WNameBuf::buf() {
  return {data(), size()};
}

ArrayRef<wchar_t> WNameBuf::buf() const {
  return {data(), size()};
}
