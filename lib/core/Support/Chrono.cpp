//===- Support/Chrono.hpp -------------------------------------------===//
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

#include <Support/Chrono.hpp>
#include <Common/Features.hpp>
#include <Support/FmtBuffer.hpp>
#include <Support/raw_ostream.hpp>
#include <fmt/format.h>
#include <fmt/chrono.h>

using namespace exi;
using namespace exi::sys;

const char exi::H::unit<std::ratio<3600>>::value[] = "h";
const char exi::H::unit<std::ratio<60>>::value[] = "m";
const char exi::H::unit<std::ratio<1>>::value[] = "s";
const char exi::H::unit<std::milli>::value[] = "ms";
const char exi::H::unit<std::micro>::value[] = "us";
const char exi::H::unit<std::nano>::value[] = "ns";

//======================================================================//
// Print Implementation
//======================================================================//

static constexpr usize maxPrintSize = sizeof("YYYY-MM-DD HH:MM:SS.XXXXXXXXX");

static inline struct tm getStructTMUtc(UtcTime<> TP) {
  struct tm Storage;
  std::time_t OurTime = toTimeT(TP);

#if EXI_ON_UNIX
  struct tm *LT = ::gmtime_r(&OurTime, &Storage);
  assert(LT);
  (void)LT;
#elif defined(_WIN32)
  int Error = ::gmtime_s(&Storage, &OurTime);
  assert(!Error);
  (void)Error;
#endif

  return Storage;
}

raw_ostream& exi::operator<<(raw_ostream& OS, const sys::TimePoint<>& D) {
  StaticFmtBuffer<maxPrintSize> Buf;
  return OS << Buf("{}", D);
}

raw_ostream& exi::H::print_duration(
 raw_ostream& OS, intmax_t V, const char* Unit) {
  return OS << V << Unit;
}

raw_ostream& exi::H::print_duration(
 raw_ostream& OS, double D, const char* Unit) {
  return OS << D << Unit;
}
