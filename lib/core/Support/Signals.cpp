//===- Support/Signals.cpp -------------------------------------------===//
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
// This file defines some helpful functions for dealing with the
// possibility of unix signals occurring while your program is running.
//
//===----------------------------------------------------------------===//

#include <Support/Signals.hpp>
#include <Common/StrRef.hpp>
#include <Common/STLExtras.hpp>
#include <Common/Vec.hpp>
#include <Config/Config.inc>
// #include <Support/CommandLine.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/ErrorOr.hpp>
#include <Support/FileSystem.hpp>
// #include <Support/FileUtilities.hpp>
#include <Support/Format.hpp>
// #include <Support/FormatVariadic.hpp>
#include <Support/ManagedStatic.hpp>
// #include <Support/MemoryBuffer.hpp>
#include <Support/Path.hpp>
#include <Support/Program.hpp>
#include <Support/StringSaver.hpp>
#include <Support/raw_ostream.hpp>
#include <array>
#include <cmath>

#include "DebugOptions.hpp"

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

using namespace exi;

static ManagedStatic<String> CrashDiagnosticsDirectory;

void exi::initSignalsOptions() {
  *CrashDiagnosticsDirectory;
}

// Callbacks to run in signal handler must be lock-free because a signal handler
// could be running as we add new callbacks. We don't add unbounded numbers of
// callbacks, an array is therefore sufficient.
struct CallbackAndCookie {
  sys::SignalHandlerCallback Callback;
  void *Cookie;
  enum class Status { Empty, Initializing, Initialized, Executing };
  std::atomic<Status> Flag;
};

static constexpr usize MaxSignalHandlerCallbacks = 8;

// A global array of CallbackAndCookie may not compile with
// -Werror=global-constructors in c++20 and above
static auto CallBacksToRun()
 -> std::array<CallbackAndCookie, MaxSignalHandlerCallbacks>& {
  static std::array<CallbackAndCookie, MaxSignalHandlerCallbacks> callbacks;
  return callbacks;
}

// Signal-safe.
void sys::RunSignalHandlers() {
  for (CallbackAndCookie &RunMe : CallBacksToRun()) {
    auto Expected = CallbackAndCookie::Status::Initialized;
    auto Desired = CallbackAndCookie::Status::Executing;
    if (!RunMe.Flag.compare_exchange_strong(Expected, Desired))
      continue;
    (*RunMe.Callback)(RunMe.Cookie);
    RunMe.Callback = nullptr;
    RunMe.Cookie = nullptr;
    RunMe.Flag.store(CallbackAndCookie::Status::Empty);
  }
}

// Signal-safe.
static void insertSignalHandler(sys::SignalHandlerCallback FnPtr,
                                void *Cookie) {
  for (CallbackAndCookie &SetMe : CallBacksToRun()) {
    auto Expected = CallbackAndCookie::Status::Empty;
    auto Desired = CallbackAndCookie::Status::Initializing;
    if (!SetMe.Flag.compare_exchange_strong(Expected, Desired))
      continue;
    SetMe.Callback = FnPtr;
    SetMe.Cookie = Cookie;
    SetMe.Flag.store(CallbackAndCookie::Status::Initialized);
    return;
  }
  report_fatal_error("too many signal callbacks already registered");
}

static bool findModulesAndOffsets(void **StackTrace, int Depth,
                                  const char **Modules, intptr_t *Offsets,
                                  const char *MainExecutableName,
                                  StringSaver &StrPool);

/// Format a pointer value as hexadecimal. Zero pad it out so its always the
/// same width.
static FormattedNumber format_ptr(void *PC) {
  // Each byte is two hex digits plus 2 for the 0x prefix.
  unsigned PtrWidth = 2 + 2 * sizeof(void *);
  return format_hex((uint64_t)PC, PtrWidth);
}

static bool printMarkupContext(raw_ostream &OS, const char *MainExecutableName);

EXI_USED static bool printMarkupStackTrace(
 StrRef Argv0, void **StackTrace, int Depth, raw_ostream &OS)
{
  String MainExecutableName =
      sys::fs::exists(Argv0) ? String(Argv0)
                             : sys::fs::getMainExecutable(nullptr, nullptr);
  if (!printMarkupContext(OS, MainExecutableName.c_str()))
    return false;
  for (int I = 0; I < Depth; I++) {
    const auto Ptr = uptr(StackTrace[I]);
    OS << "{{{" << format("bt:{}:{:#016x}\n", I, Ptr) << "}}}";
  }

  return true;
}

// Include the platform-specific parts of this class.
#if EXI_ON_UNIX
#include "Unix/Signals.impl"
#endif
#ifdef _WIN32
#include "Windows/Signals.impl"
#endif
