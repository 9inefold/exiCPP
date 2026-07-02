//===- Support/Stacktrace.cpp ---------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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
/// This file provides a wrapper for cpptrace.
///
//===----------------------------------------------------------------===//

#include <Support/Stacktrace.hpp>
#if EXI_ENABLE_STACKTRACES
#include <Common/SmallVec.hpp>
#include <Common/StringExtras.hpp>
#include <Config/Config.inc>
#include <Support/Format.hpp>
#include <Support/Signals.hpp>
#include <Support/WithColor.hpp>
#include <Support/raw_ostream.hpp>
#if EXI_CPPTRACE_DEMANGLE_WITH_NOTHING
# include <Support/RTTI.hpp>
#endif

using namespace exi;
using namespace exi::trace;

StackTrace exi::ResolveCpptraceStackTrace(const SmallVecImpl<sys::StackFrame>& Frames) {
  cpptrace::raw_trace Raw;
  Raw.frames.assign(Frames.begin(), Frames.end());
  return Raw.resolve();
}

static StrRef TrimStackTraceFileName(StrRef Name, sys::StackTraceOptions Opts) {
  // TODO: Convert path separators?
  if (!Opts.TrimFileNames || Name.empty())
    return Name;
  // Handle trimming name
  bool CheckedOnce = false;
  usize Ix = Name.size();
  while (--Ix > 0) {
    if (Name[Ix] == '/' || Name[Ix] == '\\') {
      if (CheckedOnce)
        return Name.substr(Ix + 1);
      CheckedOnce = true;
    }
  }
  return Name;
}

ALWAYS_INLINE static void PrintCpptraceFrameImpl(raw_ostream& OS,
                                                 sys::StackTraceOptions Opts,
                                                 const StackFrame& Frame) {
  using enum raw_ostream::Colors;

  const bool isMultiline = Opts.PrintMultiline;
  int printCount = 0;
  auto separator = [isMultiline, &printCount] () -> StrRef {
    const int N = printCount++;
    if (N == 0)
      return "";
    else if (N == 1)
      return isMultiline ? ":\n  " : ": ";
    else
      return isMultiline ? "\n  " : ", ";
  };
  
  const u64 PC = Frame.raw_address;
  ObjectFrame M = Frame.get_object_info();
  const bool hasNoObjectInfo = !M.object_address && !Frame.is_inline;
  if (Opts.PrintPC || hasNoObjectInfo) {
    if (isMultiline)
      OS << "At ";
    if (!Frame.is_inline) {
      if constexpr (sizeof(void*) == 8)
        OS << BRIGHT_WHITE << format("0x{:016X}", PC) << RESET;
      else if constexpr (sizeof(void*) == 4)
        OS << BRIGHT_WHITE << format("0x{:08X}", static_cast<u32>(PC)) << RESET;
    } else /*inlined*/ {
      constexpr StrRef InlinedMsg = "(inlined)";
      if (!isMultiline) {
        if constexpr (sizeof(void*) == 8)
          OS << BRIGHT_WHITE << format("{: <18}", InlinedMsg) << RESET;
        else if constexpr (sizeof(void*) == 4)
          OS << BRIGHT_WHITE << format("{: <10}", InlinedMsg) << RESET;
      } else {
          OS << BRIGHT_WHITE << InlinedMsg << RESET;
      }
    }
    ++printCount;
  }

  // Verify the PC belongs to a module in this process.
  if (hasNoObjectInfo) {
    OS << BRIGHT_YELLOW << " <unknown module>" << RESET << '\n';
    return;
  }

  if (Opts.PrintModule && !Frame.is_inline) {
    OS << separator();
    // Print module name
    if (isMultiline)
      OS << "In ";
    OS << BRIGHT_YELLOW << TrimStackTraceFileName(M.object_path, Opts) << RESET;
    // Print module location
    if (Opts.PrintModuleLocation) {
      const u64 BaseOfImage = PC - M.object_address;
      OS << BRIGHT_BLUE << format("(0x{:016X})", BaseOfImage) << RESET;
      // Print module offset
      if (Opts.PrintModuleOffset) {
        const u64 disp = M.object_address;
        OS << " + " << BRIGHT_BLUE
           << format("0x{:X}", static_cast<long long>(disp))
           << RESET << " byte(s)";
      }
    }
  }

  if (Opts.PrintLocation) {
    OS << separator();
    auto filename = TrimStackTraceFileName(Frame.filename, Opts);
    OS << BRIGHT_YELLOW << filename << RESET;
    if (Frame.line.has_value()) {
      OS << ':' << BLUE << Frame.line.value() << RESET;
      if (Frame.column.has_value())
        OS << ':' << BLUE << Frame.column.value() << RESET;
    }
  }

  if (Opts.PrintFunction) {
#if EXI_CPPTRACE_DEMANGLE_WITH_NOTHING
    // Print the actual name.
    OS << separator() << BRIGHT_GREEN;
    StrRef symbolName = Frame.symbol;
    bool shouldPrintNormally = !Opts.DemangleFunctionName;
    if (Opts.DemangleFunctionName) {
      auto status = rtti::demangle(symbolName, OS);
      if (status != rtti::RttiError::Success)
        shouldPrintNormally = true;
    }
    // Handle demangle failure
    if (shouldPrintNormally)
      OS << symbolName;
    OS << RESET;
#else
    // Print the symbol name.
    OS << separator() << BRIGHT_GREEN
       << Frame.symbol << RESET;
#endif
    // Print the offset
    if (Opts.PrintFunctionOffset) {
      // ...
    }
  }
}

void exi::PrintCpptraceStackTraceFrame(raw_ostream& OS,
                                       const sys::StackTraceOptions& Opts,
                                       const StackFrame& Frame) {
  PrintCpptraceFrameImpl(OS, Opts, Frame);
}

static void PrintCpptraceTrace(raw_ostream& OS,
                               sys::StackTraceOptions Opts,
                               const StackTrace& Trace) {
  WithColor Save(OS);
  exi::buffer_ostream BufOS(OS);
  if (Opts.ColoredOutput)
    BufOS.enable_colors(OS.colors_enabled());
  
  ListSeparator LS("\n");
  for (const StackFrame& Frame : Trace) {
    BufOS << LS;
    PrintCpptraceFrameImpl(BufOS, Opts, Frame);
  }
}

raw_ostream& exi::operator<<(raw_ostream& OS, const StackTrace& Trace) {
  sys::StackTraceOptions Opts = sys::GetGlobalStackTraceOptions();
  PrintCpptraceTrace(OS, Opts, Trace);
  return OS;
}

#endif
