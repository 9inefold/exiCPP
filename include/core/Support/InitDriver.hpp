//===- Support/InitDriver.hpp ---------------------------------------===//
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
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/SmallVec.hpp>
#include <Common/Features.hpp>
#include <Common/Option.hpp>
#include <Support/Allocator.hpp>
//#include <Support/PrettyStackTrace.hpp>

namespace exi {
/// The `main()` functions in drivers start with `InitDriver` which does the
/// following one-time initializations:
///
///  1. Setting up a signal handler so that pretty stack trace is printed out
///     if a process crashes. A signal handler that exits when a failed write to
///     a pipe occurs may optionally be installed: this is on-by-default.
///
///  2. Set up the global new-handler which is called when a memory allocation
///     attempt fails.
///
///  3. Check the environment variables for color/debugging flags. These will be
///     used to enable ANSI color on Windows, and set debugging flags in dev.
///
///  4. If running on Windows, obtain command line arguments using a
///     multibyte character-aware API and convert arguments into UTF-8
///     encoding, so that you can assume that command line arguments are
///     always encoded in UTF-8 on any platform.
///
/// `InitDriver` calls `exi_shutdown()` on destruction, which cleans up
/// `ManagedStatic` objects.
class InitDriver {
public:
  InitDriver(int& Argc, const char**& Argv,
             bool InstallPipeSignalExitHandler = true);
  InitDriver(int& Argc, char**& Argv, bool InstallPipeSignalExitHandler = true)
      : InitDriver(Argc, const_cast<const char**&>(Argv),
                   InstallPipeSignalExitHandler) {}

  ~InitDriver();

private:
  BumpPtrAllocator Alloc;
  SmallVec<const char*, 0> Args;
  //Option<PrettyStackTraceProgram> StackPrinter;
};
} // namespace exi
