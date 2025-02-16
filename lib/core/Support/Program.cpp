//===- Support/Program.cpp ------------------------------------------===//
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
//  This file implements the operating system Program concept.
//
//===----------------------------------------------------------------===//

#include <Support/Program.hpp>
#include <Common/StrRef.hpp>
#include <Support/raw_ostream.hpp>

using namespace exi;
using namespace exi::sys;

//===----------------------------------------------------------------------===//
//=== WARNING: Implementation here must contain only TRULY operating system
//===          independent code.
//===----------------------------------------------------------------------===//

static bool Execute(ProcessInfo &PI, StrRef Program,
                    ArrayRef<StrRef> Args,
                    Option<ArrayRef<StrRef>> Env,
                    ArrayRef<Option<StrRef>> Redirects,
                    unsigned MemoryLimit, String *ErrMsg,
                    BitVector *AffinityMask, bool DetachProcess);

int sys::ExecuteAndWait(StrRef Program, ArrayRef<StrRef> Args,
                        Option<ArrayRef<StrRef>> Env,
                        ArrayRef<Option<StrRef>> Redirects,
                        unsigned SecondsToWait, unsigned MemoryLimit,
                        String *ErrMsg, bool *ExecutionFailed,
                        Option<ProcessStatistics> *ProcStat,
                        BitVector *AffinityMask) {
  assert(Redirects.empty() || Redirects.size() == 3);
  ProcessInfo PI;
  if (Execute(PI, Program, Args, Env, Redirects, MemoryLimit, ErrMsg,
              AffinityMask, /*DetachProcess=*/false)) {
    if (ExecutionFailed)
      *ExecutionFailed = false;
    ProcessInfo Result = Wait(
        PI, SecondsToWait == 0
          ? std::nullopt
          // Even though P1814 was approved, the alias deduction does not work
          // on clang, so we have to be explicit here.
          : Option<unsigned>(SecondsToWait),
        ErrMsg, ProcStat);
    return Result.ReturnCode;
  }

  if (ExecutionFailed)
    *ExecutionFailed = true;

  return -1;
}

ProcessInfo sys::ExecuteNoWait(StrRef Program, ArrayRef<StrRef> Args,
                               Option<ArrayRef<StrRef>> Env,
                               ArrayRef<Option<StrRef>> Redirects,
                               unsigned MemoryLimit, String *ErrMsg,
                               bool *ExecutionFailed, BitVector *AffinityMask,
                               bool DetachProcess) {
  assert(Redirects.empty() || Redirects.size() == 3);
  ProcessInfo PI;
  if (ExecutionFailed)
    *ExecutionFailed = false;
  if (!Execute(PI, Program, Args, Env, Redirects, MemoryLimit, ErrMsg,
               AffinityMask, DetachProcess))
    if (ExecutionFailed)
      *ExecutionFailed = true;

  return PI;
}

bool sys::commandLineFitsWithinSystemLimits(StrRef Program,
                                            ArrayRef<const char *> Args) {
  SmallVec<StrRef, 8> StrRefArgs;
  StrRefArgs.reserve(Args.size());
  for (const char *A : Args)
    StrRefArgs.emplace_back(A);
  return commandLineFitsWithinSystemLimits(Program, StrRefArgs);
}

void sys::printArg(raw_ostream &OS, StrRef Arg, bool Quote) {
  const bool Escape = Arg.find_first_of(" \"\\$") != StrRef::npos;

  if (!Quote && !Escape) {
    OS << Arg;
    return;
  }

  // Quote and escape. This isn't really complete, but good enough.
  OS << '"';
  for (const auto c : Arg) {
    if (c == '"' || c == '\\' || c == '$')
      OS << '\\';
    OS << c;
  }
  OS << '"';
}

// Include the platform-specific parts of this class.
#if EXI_ON_UNIX
#include "Unix/Program.impl"
#endif
#ifdef _WIN32
#include "Windows/Program.impl"
#endif
