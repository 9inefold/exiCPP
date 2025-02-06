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
#include <Support/Alignment.hpp>
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
#include <malloc.h>
#include <windows.h>

// TODO: Tests!!

using namespace exi;
using namespace exi::sys;

#if EXI_USE_MIMALLOC
bool ITestMimallocRedirect(usize Mul) {
  bool Result = true;
  if (!mi_is_redirected())
    return true;
  {
    void* Alloc = malloc(16 * Mul);
    if (void* New = _expand(Alloc, 32 * Mul))
      free(New);
    else
      free(Alloc);
  } {
    void* Alloc = malloc(16 * Mul);
    void* New = realloc(Alloc, 512 * Mul);
    free(New);
  } {
    // void* Alloc = malloc(16 * Mul);
    // void* New = _recalloc(Alloc, 64, 4 * Mul);
    // free(New);
  } {
    constexpr Align A = 32_align;
    void* Alloc = _aligned_malloc(16 * Mul, A.value());
    if (!isAddrAligned(A, Alloc))
      Result = false;
    void* New = _aligned_realloc(Alloc, 64 * Mul, A.value());
    if (!isAddrAligned(A, Alloc))
      Result = false;
    _aligned_free(New);
  }
  return Result;
}

bool testMimallocRedirect() {
  constexpr usize kMaxMul = 20'000'000;
  usize Mul = 1;

  bool Result = true;
  outs() << "Running tests...\n";
  while (Mul < kMaxMul) {
    if (ITestMimallocRedirect(Mul)) {
      outs() << "Test " << Mul << " passed.\n"; 
    } else {
      outs() << "Test " << Mul << " failed.\n"; 
      Result = false;
    }
    Mul *= 2;
  }

  if (Result)
    outs() << "All tests passed!\n"; 
  return Result;
}

void printIfInHeap(const void* Ptr) {
  if (mi_is_in_heap_region(Ptr))
    fmt::println("\"{}\" in heap!", Ptr);
  else
    fmt::println("\"{}\" not in heap!", Ptr);
}
#else
# define printIfInHeap(...) (void(0))
#endif // EXI_USE_MIMALLOC

int main(int Argc, char* Argv[]) {
#if EXI_USE_MIMALLOC
  mi_option_disable(mi_option_verbose);
  if (!mi_is_redirected()) {
    outs() << "\nRedirection failed.\n";
  } else {
    outs() << "\nIs redirected!\n";
    testMimallocRedirect();
  }
#endif // EXI_USE_MIMALLOC

  SmallStr<256> Str;
  fs::current_path(Str);
  outs() << Argv[0] << '\n';
  fmt::println("{}", Str);

  TimePoint<> TP = sys::now();
  fmt::println("TimePoint<>: {}", TP);
  TimePoint<> TP2 = sys::now();
  fmt::println("Duration: {}", (TP2 - TP));
  outs() << TP << ", " << TP2 << '\n';

  {
    auto* P = (char*)exi::allocate_buffer(4096, 16);
    printIfInHeap(P);
    exi::deallocate_buffer(P, 4096, 16);
  }

  std::string SStr;
  wrap_stream(SStr) << "Hello world!\nIt's me!";
  printIfInHeap(SStr.data());

  errs() << "\n\n";
  errs() << "mimalloc: " << Process::GetMallocUsage() << '\n';
  errs() << "malloc:   " << Process::GetStdMallocUsage() << '\n';
}
