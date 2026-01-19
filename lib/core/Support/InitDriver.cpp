//===- Support/InitDriver.cpp ---------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
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
/// This file provides a class to handle driver initialization.
/// See https://github.com/llvm/llvm-project/blob/main/llvm/lib/Support/InitLLVM.cpp
///
//===----------------------------------------------------------------===//

#include <Support/InitDriver.hpp>
#include <Common/StringSwitch.hpp>
//#include <Support/AutoConvert.hpp>
#include <Support/Error.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Logging.hpp>
#include <Support/ManagedStatic.hpp>
#include <Support/Process.hpp>
#include <Support/Signals.hpp>
#include <Support/D/IO.hpp>
#ifdef _WIN32
# include <Support/Windows/WindowsSupport.hpp>
#endif

#if !__has_include(<unistd.h>)
# ifndef STDIN_FILENO
#  define STDIN_FILENO 0
# endif
# ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
# endif
# ifndef STDERR_FILENO
#  define STDERR_FILENO 2
# endif
#endif

using namespace exi;
using namespace exi::sys;

static bool CheckEnvTruthiness(StrRef Env, bool EmptyResult = false) {
  if (Env.empty())
    return false;
  // Handle integral values.
  i64 Int = 0;
  if (!Env.consumeInteger(10, Int))
    return (Int != 0);
  // Parse string.
  return StringSwitch<bool>(Env)
    .Cases("TRUE", "YES", "ON", true)
    .Cases("FALSE", "NO", "OFF", false)
    .Default(EmptyResult);
}

static bool CheckEnv(StrRef EnvName, bool EmptyResult = false) {
  Option<String> Env = Process::GetEnv(EnvName);
  if (!Env)
    return EmptyResult;
  return CheckEnvTruthiness(*Env, EmptyResult);
}

static bool HandleEscapeCodeSetup() {
  if (Process::IsReallyDebugging()) {
    if (CheckEnv("EXICPP_NO_ANSI", /*EmptyResult=*/false)) {
      Process::UseANSIEscapeCodes(false);
      return false;
    }
  }

  Process::UseANSIEscapeCodes(true);
  Process::UseUTF8Codepage(true);
  return true;
}

static void HandleDebugSetup() {
#if EXI_DEBUG
  if (CheckEnv("EXICPP_TRAP_ERRORS"))
    exi::IsDebuggingFlag = true;
#endif
}

static constinit bool UseANSI = false;

void CleanupStdHandles(void*) {
  exi::raw_ostream *Outs = &exi::outs(), *Errs = &exi::errs();
  if (UseANSI) {
    static constexpr const char Reset[] = "\033[0m\n";
    Outs->write(Reset, std::strlen(Reset));
    Errs->write(Reset, std::strlen(Reset));
  }
  Outs->flush();
  Errs->flush();
}

InitDriver::InitDriver(int& Argc, const char**& Argv,
                       bool InstallPipeSignalExitHandler) {
#if EXI_ASSERTS || EXI_ENABLE_GUARDRAILS
  static std::atomic<bool> Initialized{false};
  exi_guard_assert(!Initialized, "InitDriver was already initialized!");
  Initialized = true;
#endif

  // Bring stdin/stdout/stderr into a known state.
  sys::AddSignalHandler(CleanupStdHandles, nullptr);

  if (InstallPipeSignalExitHandler)
    // The pipe signal handler must be installed before any other handlers are
    // registered. This is because the Unix \ref RegisterHandlers function does
    // not perform a sigaction() for SIGPIPE unless a one-shot handler is
    // present, to allow long-lived processes (like lldb) to fully opt-out of
    // llvm's SIGPIPE handling and ignore the signal safely.
    sys::SetOneShotPipeSignalFunction(sys::DefaultOneShotPipeSignalHandler);
  // Initialize the stack printer after installing the one-shot pipe signal
  // handler, so we can perform a sigaction() for SIGPIPE on Unix if requested.
  //StackPrinter.emplace(Argc, Argv);
  sys::PrintStackTraceOnErrorSignal(Argv[0]);
  exi::install_out_of_memory_new_handler();

  // Check if EXICPP_TRAP_ERRORS is enabled
  HandleDebugSetup();
  // Check if debugging & EXICPP_NO_ANSI is enabled
  UseANSI = HandleEscapeCodeSetup();
  // Enable/Disable colors for all streams
  outs().enable_colors(UseANSI);
  errs().enable_colors(UseANSI);
#if EXI_DEBUG
  dbgs().enable_colors(UseANSI);
#endif

#ifdef _WIN32
  // We use UTF-8 as the internal character encoding. On Windows,
  // arguments passed to main() may not be encoded in UTF-8. In order
  // to reliably detect encoding of command line arguments, we use an
  // Windows API to obtain arguments, convert them to UTF-8, and then
  // write them back to the Argv vector.
  //
  // There's probably other way to do the same thing (e.g. using
  // wmain() instead of main()), but this way seems less intrusive
  // than that.
  std::string Banner = std::string(Argv[0]) + ": ";
  ExitOnError ExitOnErr(Banner);

  ExitOnErr(errorCodeToError(windows::GetCommandLineArguments(Args, Alloc)));

  // GetCommandLineArguments doesn't terminate the vector with a
  // nullptr.  Do it to make it compatible with the real argv.
  Args.push_back(nullptr);

  Argc = Args.size() - 1;
  Argv = Args.data();
#endif
}

InitDriver::~InitDriver() {
  CleanupStdHandles(nullptr);
  exi::exi_shutdown();
}
