//===- Support/Stacktrace.hpp ---------------------------------------===//
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

#pragma once

#include <Support/D/Stacktrace-fwd.hpp>
#include <Support/D/StackFrame.hpp>
#if EXI_ENABLE_STACKTRACES
# include <cpptrace/basic.hpp>
#endif

namespace exi {
class raw_ostream;
template <typename T> class SmallVecImpl;
namespace sys { struct StackTraceOptions; }

#if EXI_ENABLE_STACKTRACES
/// Gets a `cpptrace::stacktrace` from frames captured ahead of time.
trace::StackTrace ResolveCpptraceStackTrace(const SmallVecImpl<sys::StackFrame>& Frames);
/// Prints a single `cpptrace::stacktrace_frame` to \param OS.
void PrintCpptraceStackTraceFrame(raw_ostream& OS,
                                  const sys::StackTraceOptions& Opts,
                                  const trace::StackFrame& Frame);
raw_ostream& operator<<(raw_ostream& OS, const trace::StackTrace& Trace);
#else
inline StackTrace ResolveCpptraceStackTrace(const SmallVecImpl<StackFrame>& Frames) {
  return StackTrace {};
}
inline void PrintCpptraceStackTraceFrame(raw_ostream& OS,
                                         const sys::StackTraceOptions& Opts,
                                         const trace::StackFrame& Frame) {
  // ...
}
inline raw_ostream& operator<<(raw_ostream& OS, const trace::StackTrace& Trace) {
  return OS;
}
#endif

//////////////////////////////////////////////////////////////////////////
// JIT

namespace trace {

#if EXI_ENABLE_STACKTRACES
inline void JITRegisterObject(const char* Name, usize Size) {
  return cpptrace::register_jit_object(Name, Size);
}
inline void JITUnregisterObject(const char* Name) {
  return cpptrace::unregister_jit_object(Name);
}
inline void JITClearAllObjects() {
  return cpptrace::clear_all_jit_objects();
}
#else
ALWAYS_INLINE void JITRegisterObject(const char*, usize) {}
ALWAYS_INLINE void JITUnregisterObject(const char*) {}
ALWAYS_INLINE void JITClearAllObjects() {}
#endif

} // namespace trace

} // namespace exi 
