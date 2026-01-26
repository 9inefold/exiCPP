//===- Support/Except.hpp -------------------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file defines handlers for "exceptional" cases.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StrRef.hpp>
#include <Common/Twine.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Signals.hpp>
#include <type_traits>
#include <stdexcept>

namespace exi {
namespace except_detail {
template <typename Ex>
[[noreturn]] ALWAYS_INLINE EXI_NODEBUG void ThrowImpl(const char* Msg) {
#if EXI_EXCEPTIONS
  throw Ex(Msg);
#else
  exi::report_fatal_error(Msg);
#endif
}

template <typename Ex>
[[noreturn]] ALWAYS_INLINE EXI_NODEBUG void ThrowDynImpl(const Twine& Msg) {
#if EXI_EXCEPTIONS
  throw Ex(Msg.str());
#else
  exi::report_fatal_error(Msg);
#endif
}
} // namespace H

/// Thrown for invalid arguments.
class runtime_error : public std::runtime_error {
  SmallVec<sys::StackFrame, 0> Frames;
public:
  using std::runtime_error::runtime_error;
  runtime_error(const std::string& Msg) : std::runtime_error(Msg) {
    this->captureFrames();
  }
  runtime_error(const char* Msg) : std::runtime_error(Msg) {
    this->captureFrames();
  }
  const SmallVecImpl<sys::StackFrame>& frames() const {
    return this->Frames;
  }
private:
  /// Implemented in `Support/Signals.cpp`.
  void captureFrames();
};

/// Implements "throwing" in a way that reduces register pressure.
template <typename Ex = exi::runtime_error>
[[noreturn]] EXI_ERROR_CC void Throw(StringLiteral Msg = "") {
  except_detail::ThrowImpl<Ex>(Msg.data());
}

/// Implements "throwing" in a way that reduces register pressure.
template <typename Ex = exi::runtime_error>
[[noreturn]] EXI_ERROR_CC void ThrowDyn(const Twine& Msg) {
  except_detail::ThrowDynImpl<Ex>(Msg);
}

} // namespace exi
