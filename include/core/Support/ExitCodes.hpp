//===- Support/ExitCodes.hpp -----------------------------------------===//
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
///
/// \file
/// This file contains definitions of exit codes for exit() function. They are
/// either defined by sysexits.h if it is supported, or defined here if
/// sysexits.h is not supported.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <Config/Config.inc>

#if __has_include(<sysexits.h>)
# include <sysexits.h>
#elif __MVS__ || defined(_WIN32)
// <sysexits.h> does not exist on z/OS and Windows. The only value used in LLVM
// is EX_IOERR, which is used to signal a special error condition (broken pipe).
// Define the macro with its usual value from BSD systems, which is chosen to
// not clash with more standard exit codes like 1.
# define EX_IOERR 74
#elif EXI_ON_UNIX
# error Exit code EX_IOERR not available
#endif
