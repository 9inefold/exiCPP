//===- Support/RTTI.hpp ---------------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
/// This file provides an API for getting demangled type names.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/Result.hpp>
#include <Common/StrRef.hpp>
#include <string>
#include <typeinfo>

namespace exi {

template <typename> class SmallVecImpl;

namespace rtti {

/// A common error API for various demangling implementations.
enum class RttiError : i32 {
  /// Demangling succeeded.
  Success             = 0,
  /// A memory allocation failed.
  InvalidMemoryAlloc  = -1,
  /// Invalid mangled name under the current ABI.
  InvalidName         = -2,
  /// Invalid argument passed to API.
  InvalidArgument     = -3,
  /// Unknown status code...
  Other               = -4,
};

/// The `Result` type used for demangling.
template <typename T>
using RttiResult = Result<T, RttiError>;

/// Takes the mangled `Symbol` and returns a demangled `String`.
RttiResult<String> demangle(const char* Symbol);
/// Takes the mangled `Symbol` and returns a demangled `String`.
RttiResult<String> demangle(StrRef Symbol);
#if !EXI_COMPILER(MSVC)
/// Takes the mangled `.name()` and returns a demangled `String`.
RttiResult<String> demangle(const std::type_info& Info);
#else
/// Takes the `.name()` and returns it as a `String`.
inline RttiResult<String> demangle(const std::type_info& Info) {
  return String(Info.name());
}
#endif

/// Takes the mangled `Symbol` and adds it to the buffer.
/// @note `Buf` will be cleared on success.
RttiResult<StrRef> demangle(const char* Symbol, SmallVecImpl<char>& Buf);
/// Takes the mangled `Symbol` and adds it to the buffer.
/// @note `Buf` will be cleared on success.
RttiResult<StrRef> demangle(StrRef Symbol, SmallVecImpl<char>& Buf);
/// Takes the mangled `.name()` and adds it to the buffer.
/// @note `Buf` will be cleared on success.
RttiResult<StrRef> demangle(const std::type_info& Info, SmallVecImpl<char>& Buf);

/// Gets the `typeid` of `Val` and returns its name as a `String`.
template <typename T>
EXI_INLINE RttiResult<String> name(const T& Val) {
  return demangle(typeid(Val));
}

/// Gets the `typeid` of `Val` and adds its name to the buffer.
template <typename T>
EXI_INLINE RttiResult<StrRef> name(const T& Val, SmallVecImpl<char>& Buf) {
  return demangle(typeid(Val), Buf);
}

/// Gets the `typeid` of `T` and returns its name as a `String`.
template <typename T>
EXI_INLINE RttiResult<String> name() {
  return demangle(typeid(T));
}

/// Gets the `typeid` of `T` and adds its name to the buffer.
template <typename T>
EXI_INLINE RttiResult<StrRef> name(SmallVecImpl<char>& Buf) {
  return demangle(typeid(T), Buf);
}

} // namespace rtti
} // namespace exi
