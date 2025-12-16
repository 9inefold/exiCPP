//===- Support/D/Stacktrace-Fwd.hpp ---------------------------------===//
//
// Copyright (C) 2025 Ninefold
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

#pragma once

#include <Common/Fundamental.hpp>
#include <Config/Config.inc>
#if EXI_ENABLE_STACKTRACES
# include <cpptrace/forward.hpp>
# define EXI_CPPTRACE_REGISTER_OBJ(NAME) using ::cpptrace::NAME
#else
# define EXI_CPPTRACE_REGISTER_OBJ(NAME) struct NAME {}
#endif

namespace exi {
namespace trace {

EXI_CPPTRACE_REGISTER_OBJ(raw_trace);
EXI_CPPTRACE_REGISTER_OBJ(object_trace);
EXI_CPPTRACE_REGISTER_OBJ(stacktrace);
EXI_CPPTRACE_REGISTER_OBJ(object_frame);
EXI_CPPTRACE_REGISTER_OBJ(stacktrace_frame);

using RawTrace    = raw_trace;
using ObjectTrace = object_trace;
using StackTrace  = stacktrace;

using ObjectFrame = object_frame;
using StackFrame  = stacktrace_frame;

#if EXI_ENABLE_STACKTRACES
inline void JITRegisterObject(const char*, usize);
inline void JITUnregisterObject(const char*);
inline void JITClearAllObjects();
#else
ALWAYS_INLINE void JITRegisterObject(const char*, usize) {}
ALWAYS_INLINE void JITUnregisterObject(const char*) {}
ALWAYS_INLINE void JITClearAllObjects() {}
#endif

} // namespace trace
} // namespace exi 

#undef EXI_REGISTER_CPPTRACE_OBJ
