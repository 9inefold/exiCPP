//===- Support/Process.cpp ------------------------------------------===//
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
//  This file implements the operating system Process concept.
//
//===----------------------------------------------------------------===//

#include <Support/Process.hpp>
#include <Common/Option.hpp>
#include <Common/STLExtras.hpp>
#include <Common/StringExtras.hpp>
#include <Config/Config.inc>
#include <Config/FeatureFlags.hpp>
#if EXI_HAS_CRASHRECOVERYCONTEXT
# include <Support/CrashRecoveryContext.hpp>
#endif
#include <Support/Filesystem.hpp>
#include <Support/Path.hpp>
#include <stdlib.h> // for _Exit
#if EXI_USE_MIMALLOC
# include <mimalloc.h>
#endif

#define EXI_ENABLE_CRASH_DUMPS 0

using namespace exi;
using namespace exi::sys;

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

#if EXI_USE_MIMALLOC

static bool MimallocUsageVisitor(
  const mi_heap_t* Heap,
  const mi_heap_area_t* HeapArea,
  void* Block, usize BlockSize,
  void* Input
) {
  if EXI_LIKELY(HeapArea) {
    auto& Usage = *static_cast<usize*>(Input);
    Usage += HeapArea->used * HeapArea->full_block_size;
  }
  return true;
}

#endif

usize Process::GetMallocUsage() {
#if EXI_USE_MIMALLOC
  usize Usage = 0;
  mi_heap_visit_blocks(
    mi_heap_get_default(), false,
    &MimallocUsageVisitor, &Usage
  );
  return Usage;
#else
  return Process::GetStdMallocUsage();
#endif
}

Option<String>
Process::FindInEnvPath(StrRef EnvName, StrRef FileName, char Separator) {
  return FindInEnvPath(EnvName, FileName, {}, Separator);
}

Option<String>
Process::FindInEnvPath(StrRef EnvName, StrRef FileName,
                       ArrayRef<String> IgnoreList, char Separator) {
  assert(!path::is_absolute(FileName));
  Option<String> FoundPath;
  Option<String> OptPath = Process::GetEnv(EnvName);
  if (!OptPath)
    return FoundPath;

  const char EnvPathSeparatorStr[] = {Separator, '\0'};
  SmallVec<StrRef, 8> Dirs;
  SplitString(*OptPath, Dirs, EnvPathSeparatorStr);

  for (StrRef Dir : Dirs) {
    if (Dir.empty())
      continue;

    if (any_of(IgnoreList, [&](StrRef S) { return fs::equivalent(S, Dir); }))
      continue;

    SmallStr<128> FilePath(Dir);
    path::append(FilePath, FileName);
    if (fs::exists(Twine(FilePath))) {
      FoundPath = String(FilePath);
      break;
    }
  }

  return FoundPath;
}

// clang-format off
#define COLOR(FGBG, CODE, BOLD) "\033[0;" BOLD FGBG CODE "m"

#define ALLCOLORS(FGBG, BRIGHT, BOLD) \
  {                           \
    COLOR(FGBG, "0", BOLD),   \
    COLOR(FGBG, "1", BOLD),   \
    COLOR(FGBG, "2", BOLD),   \
    COLOR(FGBG, "3", BOLD),   \
    COLOR(FGBG, "4", BOLD),   \
    COLOR(FGBG, "5", BOLD),   \
    COLOR(FGBG, "6", BOLD),   \
    COLOR(FGBG, "7", BOLD),   \
    COLOR(BRIGHT, "0", BOLD), \
    COLOR(BRIGHT, "1", BOLD), \
    COLOR(BRIGHT, "2", BOLD), \
    COLOR(BRIGHT, "3", BOLD), \
    COLOR(BRIGHT, "4", BOLD), \
    COLOR(BRIGHT, "5", BOLD), \
    COLOR(BRIGHT, "6", BOLD), \
    COLOR(BRIGHT, "7", BOLD), \
  }

//                           bg
//                           |  bold
//                           |  |   codes
//                           |  |   |
//                           |  |   |
static const char colorcodes[2][2][16][11] = {
    { ALLCOLORS("3", "9", ""), ALLCOLORS("3", "9", "1;"),},
    { ALLCOLORS("4", "10", ""), ALLCOLORS("4", "10", "1;")}
};

//                           bg
//                           |  bold
//                           |  |  codes
//                           |  |  |
//                           |  |  |
static const char resetcodes[2][2][10] = {
    {"\033[0;39m", "\033[0;1;39m"},
    {"\033[0;49m", "\033[0;1;49m"}
};
// clang-format on

// A CMake option controls wheter we emit core dumps by default. An application
// may disable core dumps by calling Process::PreventCoreFiles().
static bool coreFilesPrevented = !EXI_ENABLE_CRASH_DUMPS;

bool Process::AreCoreFilesPrevented() { return coreFilesPrevented; }

bool Process::IsDebugging() {
  return exi::DebugFlag && Process::IsReallyDebugging();
}

[[noreturn]] void Process::Exit(int RetCode, bool NoCleanup) {
#if EXI_HAS_CRASHRECOVERYCONTEXT
  if (CrashRecoveryContext *CRC = CrashRecoveryContext::GetCurrent())
    CRC->HandleExit(RetCode);
#endif

  if (NoCleanup)
    ExitNoCleanup(RetCode);
  else
    ::exit(RetCode);
}

// Include the platform-specific parts of this class.
#if EXI_ON_UNIX
#include "Unix/Process.impl"
#endif
#ifdef _WIN32
#include "Windows/Process.impl"
#endif
