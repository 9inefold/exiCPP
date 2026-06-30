//===- Common/InlineArray.hpp ---------------------------------------===//
//
// Copyright (C) 2026 Ninefold
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
/// This file implements an array with the size inline.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/InlineArray.hpp>
#include <Common/StrRef.hpp>

namespace exi {

/// An array with the size allocated inline.
/// Memory is allocated with the following layout:
///
///   [ Size ][ Elements ]
///
/// This allows for more convenient inline arrays.
struct InlineStr final
    : public InlineArrayCommon<InlineStr, char> {
  using BaseT = InlineArrayCommon<InlineStr, char>;
  using value_type = char;
  /// As small as possible to pack things tightly.
  using size_type = u16;
  size_type Size;
  char Data[];

public:
  InlineStr(const InlineStr&) = delete;
  InlineStr& operator=(const InlineStr&) = delete;
  InlineStr(InlineStr&&) = delete;
  InlineStr& operator=(InlineStr&&) = delete;

  using BaseT::New;
  using BaseT::Delete;

  EXI_INLINE static InlineStr* New(StrRef Data) {
    return New(Data.data(), Data.size());
  }
  template <class AllocatorT>
  EXI_INLINE static InlineStr* New(AllocatorT& Alloc, StrRef Data) {
    return New(Alloc, Data.data(), Data.size());
  }

  ALWAYS_INLINE StrRef str() const { return StrRef(Data, Size); }
  MutArrayRef<char> arr() { return MutArrayRef(Data, Size); }
  ArrayRef<char> arr() const { return ArrayRef(Data, Size); }
  usize size() const { return Size; }
  usize size_in_bytes() const { return Size; }
};

extern template struct InlineArrayCommon<InlineStr, char>;

/// Allocates a new `InlineStr`.
InlineStr* make_inlinestr(StrRef S, bool NullTerminate = true);

/// Allocates a new `InlineStr`.
template <class AllocatorT>
InlineStr* make_inlinestr(AllocatorT& Alloc, StrRef S, bool NullTerminate = true) {
  if (!NullTerminate)
    return InlineStr::New(Alloc, S);
  auto* Out = InlineStr::NewUninit(Alloc, S.size() + 1);
  Out->Size = S.size();
  FastUninitCopy(Out->Data, S.data(), S.size());
  Out->Data[S.size()] = '\0';
  return Out;
}

} // namespace exi
