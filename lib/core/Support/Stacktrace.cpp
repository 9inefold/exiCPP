//===- Support/Stacktrace.cpp ---------------------------------------===//
//
// Copyright (C) 2024-2025 Eightfold
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
#include <Support/raw_ostream.hpp>

using namespace exi;
using namespace exi::trace;

#if EXI_ENABLE_STACKTRACES
raw_ostream& exi::operator<<(raw_ostream& OS, const StackTrace& Trace) {
  return OS << Trace.to_string(OS.colors_enabled());
}
#endif
