//===- Driver.cpp ---------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
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

#include <Common/Box.hpp>
#include <Common/Map.hpp>
#include <Common/String.hpp>
#include <Common/SmallStr.hpp>
#include <Common/SmallVec.hpp>
#include <Common/StringSwitch.hpp>
#include <Support/Allocator.hpp>
#include <Support/Chrono.hpp>
#include <Support/Casting.hpp>
#include <Support/Error.hpp>
#include <Support/Filesystem.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/MemoryBufferRef.hpp>
#include <Support/Process.hpp>
#include <Support/raw_ostream.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <tuple>
#include <windows.h>

using namespace exi;
using namespace exi::sys;

extern "C" {
__declspec(dllimport) bool mi_allocator_init(const char** Str);
}

int main(int Argc, char* Argv[]) {
  {
    mi_option_disable(mi_option_verbose);
    const char* Msgs = nullptr;
    mi_allocator_init(&Msgs);
    outs() << "Messages:\n" << Msgs;
    return 0;
  }

  SmallStr<256> Str;
  fs::current_path(Str);
  // fmt::println("{}", Str);
  outs() << Argv[0] << '\n';
  outs() << Str << '\n';

  // TODO: Test raw_ostream
  TimePoint<> TP = sys::now();
  fmt::println("TimePoint<>: {}", TP);
  TimePoint<> TP2 = sys::now();
  fmt::println("Duration: {}", (TP2 - TP));
  outs() << TP << ", " << TP2 << '\n';

  {
    auto* P = (char*)exi::allocate_buffer(4096, 16);
    if (mi_is_in_heap_region(P))
      fmt::println("In heap!");
    exi::deallocate_buffer(P, 4096, 16);
  }

  if (Error E = createStringError(
   exi::inconvertibleErrorCode(), "X: {}", 1)) {
    handleAllErrors(std::move(E), [](StringError& SE) {
      fmt::println("Error: `{}`", SE.getMessage());
    });
  }

  SmallStr<256> Inl {};
  {
    raw_svector_ostream V(Inl);
    V << "hello" << ' ' << "world" << '!';
  }

  int I = StringSwitch<int>(Inl.str())
    .Case("Hello",            0)
    .CaseLower("Hello",       1)
    .StartsWith("Hello",      2)
    .StartsWithLower("Hello", 3)
    .Default(4);
  exi_assert(I == 3, "StringSwitch failed!");
  fmt::println("{}", I);

  SmallStr<4> SS;
  // exi::wrap_stream(SS) << "Hello" << ' ' << "world" << '!';

  String StdStr;
  exi::wrap_stream(StdStr) << "Hello" << ' ' << "world" << '!';
  if (mi_is_in_heap_region(StdStr.data()))
    fmt::println("String in heap!");

  errs() << "\n\n";
  errs() << "mimalloc: " << Process::GetMallocUsage() << '\n';
  errs() << "malloc:   " << Process::GetStdMallocUsage() << '\n';
}
