//===- Support/Debug.cpp ---------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
//
// This file implements a handy way of adding debugging information to your
// code, without it being enabled all of the time, and without having to add
// command line options to enable it.
//
// In particular, just wrap your code with the DEBUG_ONLY() macro, and it will
// be enabled automatically if you specify '-debug' on the command-line.
// DEBUG_ONLY() requires the DEBUG_TYPE macro to be defined. Set it to "foo"
// specify that your debug code belongs to class "foo". Be careful that you only
// do this after including Debug.h and not around any #include of headers.
// Headers should define and undef the macro acround the code that needs to use
// the DEBUG_ONLY() macro. Then, on the command line, you can specify
// '-debug-only=foo' to enable JUST the debug information for the foo class.
//
// When compiling without assertions, the -debug-* options and all code in
// DEBUG_ONLY() statements disappears, so it does not affect the runtime of the
// code.
//
//===----------------------------------------------------------------===//

#include <Support/Debug.hpp>
#include <Common/StrRef.hpp>
#include <Common/Vec.hpp>
#include <Support/ManagedStatic.hpp>
#include <Support/Signals.hpp>
#include <Support/circular_raw_ostream.hpp>
#include <Support/raw_ostream.hpp>
// #include "DebugOptions.h"

#undef isCurrentDebugType
#undef setCurrentDebugType
#undef setCurrentDebugTypes

using namespace exi;

// Even though LLVM might be built with NDEBUG, define symbols that the code
// built without NDEBUG can depend on via the llvm/Support/Debug.h header.
namespace exi {
/// Exported boolean set by the -debug option.
bool DebugFlag = false;

static ManagedStatic<Vec<String>> CurrentDebugType;

static bool IsEmptyCStr(const char *Str) noexcept {
  if (Str == nullptr)
    return true;
  return *Str == '\0';
}

/// Return true if the specified string is the debug type
/// specified on the command line, or if none was specified on the command line
/// with the -debug-only=X option.
bool exi::isCurrentDebugType(const char *DebugType) {
  if (CurrentDebugType->empty())
    return true;
  StrRef DebugTypeStr(DebugType);
  // See if DebugType is in list. Note: do not use find() as that forces us to
  // unnecessarily create a String instance.
  for (auto& D : *CurrentDebugType) {
    if (DebugTypeStr.equals(D))
      return true;
  }
  return false;
}

/// Set the current debug type, as if the -debug-only=X
/// option were specified.  Note that DebugFlag also needs to be set to true for
/// debug output to be produced.
///
void setCurrentDebugTypes(const char **Types, unsigned Count);

void exi::setCurrentDebugType(const char *Type) {
  setCurrentDebugTypes(&Type, 1);
}

void exi::setCurrentDebugTypes(const char **Types, unsigned Count) {
  CurrentDebugType->clear();
  for (usize T = 0; T < Count; ++T) {
    const char *const Type = Types[T];
    if (IsEmptyCStr(Type))
      continue;
    CurrentDebugType->push_back(Type);
  }
}

} // namespace exi

// All Debug.h functionality is a no-op in NDEBUG mode.
#if EXI_DEBUG

// Signal handlers - dump debug output on termination.
static void debug_user_sig_handler(void *Cookie);

namespace {
struct dbgstream {
  circular_raw_ostream Strm;

  dbgstream()
      : Strm(errs(), "*** Debug Log Output ***\n",
             (!EnableDebugBuffering || !DebugFlag) ? 0 : *DebugBufferSize) {
    if (EnableDebugBuffering && DebugFlag && *DebugBufferSize != 0)
      // TODO: Add a handler for SIGUSER1-type signals so the user can
      // force a debug dump.
      sys::AddSignalHandler(&debug_user_sig_handler, nullptr);
    // Otherwise we've already set the debug stream buffer size to
    // zero, disabling buffering so it will output directly to errs().
  }
};
} // `namespace anonymous`

// Signal handlers - dump debug output on termination.
static void debug_user_sig_handler(void *Cookie) {
  // This is a bit sneaky.  Since this is under #if EXI_DEBUG, we
  // know that debug mode is enabled and dbgs() really is a
  // circular_raw_ostream.  If EXI_DEBUG is defined, then dbgs() ==
  // errs() but this will never be invoked.
  exi::circular_raw_ostream &dbgout =
      static_cast<circular_raw_ostream &>(exi::dbgs());
  dbgout.flushBufferWithBanner();
}

/// dbgs - Return a circular-buffered debug stream.
raw_ostream &exi::dbgs() {
  // Do one-time initialization in a thread-safe way.
  static dbgstream thestrm;
  return thestrm.Strm;
}

#else
// Avoid "has no symbols" warning.
namespace exi {
/// dbgs - Return errs().
raw_ostream &dbgs() {
  return exi::errs();
}
} // namespace exi
#endif

/// EnableDebugBuffering - Turn on signal handler installation.
///
bool exi::EnableDebugBuffering = false;
