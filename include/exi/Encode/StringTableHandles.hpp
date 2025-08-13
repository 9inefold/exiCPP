//===- exi/Encode/StringTableHandles.hpp -----------------------------===//
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
/// This file implements the opaque handles for the encoder's StringTable.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/OpaqueHandle.hpp>
#include <core/Support/PointerLikeTraits.hpp>

namespace exi::encode {

class StringTable;

// TODO: Enable macro expansion for EXI_OPAQUE_HANDLE[_T]
// See https://stackoverflow.com/questions/42300539/documenting-macros-using-doxygen

/// @typedef STPrefixEntry
/// Typed handle for `StringTable::PrefixEntry`.
using STPrefixEntry = EXI_OPAQUE_HANDLE_T(STPrefixEntry, StringTable);
/// @typedef STURIEntry
/// Typed handle for `StringTable::URIEntry`.
using STURIEntry = EXI_OPAQUE_HANDLE_T(STURIEntry, StringTable);
/// @typedef LocalNameInsert
/// Insertion point for a LocalName.
using LocalNameInsert = EXI_OPAQUE_HANDLE_T(LocalNameInsert, StringTable);
//struct STURIEntry;
/// @typedef STValueEntry
/// Typed handle for `StringTable::ValueEntry`.
using STValueEntry = EXI_OPAQUE_HANDLE_T(STValueEntry, StringTable);
/// @typedef QualName
/// Handle for an `InlineString` representing a QName's data as `"URI$ln"`.
using QualName = EXI_OPAQUE_HANDLE_T(QualName, StringTable);

namespace H {
enum : int {
  kSTEntryBaseAlignment = sizeof(usize),
  kSTEntryLowBits = exi::H::ConstantLog2<kSTEntryBaseAlignment>::value,
};
} // namespace H

} // namespace exi::encode

//////////////////////////////////////////////////////////////////////////
// Traits

namespace exi {

// TODO: Add OpaquePointerLikeTypeTraits?

template <> struct PointerLikeTypeTraits<encode::STPrefixEntry*> {
  using T = encode::STPrefixEntry;
  static inline void* getAsVoidPointer(T* P) { return P; }
  static inline T* getFromVoidPointer(void* P) { return static_cast<T*>(P); }
  static constexpr int NumLowBitsAvailable = encode::H::kSTEntryLowBits;
};
template <> struct PointerLikeTypeTraits<encode::STURIEntry*> {
  using T = encode::STURIEntry;
  static inline void* getAsVoidPointer(T* P) { return P; }
  static inline T* getFromVoidPointer(void* P) { return static_cast<T*>(P); }
  static constexpr int NumLowBitsAvailable = encode::H::kSTEntryLowBits;
};
template <> struct PointerLikeTypeTraits<encode::LocalNameInsert*> {
  using T = encode::LocalNameInsert;
  static inline void* getAsVoidPointer(T* P) { return P; }
  static inline T* getFromVoidPointer(void* P) { return static_cast<T*>(P); }
  static constexpr int NumLowBitsAvailable = H::ConstantLog2<4>::value;
};
template <> struct PointerLikeTypeTraits<encode::STValueEntry*> {
  using T = encode::STValueEntry;
  static inline void* getAsVoidPointer(T* P) { return P; }
  static inline T* getFromVoidPointer(void* P) { return static_cast<T*>(P); }
  static constexpr int NumLowBitsAvailable = encode::H::kSTEntryLowBits;
};
template <> struct PointerLikeTypeTraits<encode::QualName*> {
  using T = encode::QualName;
  static inline void* getAsVoidPointer(T* P) { return P; }
  static inline T* getFromVoidPointer(void* P) { return static_cast<T*>(P); }
  static constexpr int NumLowBitsAvailable = H::ConstantLog2<2>::value;
};

} // namespace exi
