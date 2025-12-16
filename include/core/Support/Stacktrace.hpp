//===- Support/Stacktrace.hpp ---------------------------------------===//
//
// Copyright (C) 2024-2025 Ninefold
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
#if EXI_ENABLE_STACKTRACES
# include <cpptrace/basic.hpp>
#endif

namespace exi {
class raw_ostream;
namespace trace {

#if EXI_ENABLE_STACKTRACES
EXI_NO_INLINE inline StackTrace GetTrace(usize Skip = 0) {
  return cpptrace::generate_trace(Skip + 1);
}
EXI_NO_INLINE inline StackTrace GetTrace(usize Skip, usize MaxDepth) {
  return cpptrace::generate_trace(Skip + 1, MaxDepth);
}
#else
ALWAYS_INLINE StackTrace GetTrace(usize = 0) { return {}; }
ALWAYS_INLINE StackTrace GetTrace(usize, usize) { return {}; }
#endif

//////////////////////////////////////////////////////////////////////////
// JIT

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
#endif

} // namespace trace

#if EXI_ENABLE_STACKTRACES
raw_ostream& operator<<(raw_ostream& OS, const trace::StackTrace& Trace);
#else
inline raw_ostream& operator<<(raw_ostream& OS,
                               const trace::StackTrace&) { return OS; }
#endif

} // namespace exi 
