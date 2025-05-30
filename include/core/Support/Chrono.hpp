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
//
// This file contains some utilities for timing info.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <Support/Ratio.hpp>
#include <chrono>
#include <ctime>

namespace exi {

class raw_ostream;

template <typename> struct TimePointUtil;

namespace H {
template <typename Rep>
using TimePointRep = std::conditional_t<
  std::chrono::treat_as_floating_point_v<Rep>,
  double, intmax_t>;
} // namespace H

namespace sys {

template <typename Rep, typename Period>
using StdTime = std::chrono::time_point<Rep, Period>;

using SystemClock = std::chrono::system_clock;

/// A time point on the system clock. This is provided for two reasons:
/// - to insulate us against subtle differences in behavior to differences in
///   system clock precision (which is implementation-defined and differs
///   between platforms).
/// - to shorten the type name
/// The default precision is nanoseconds. If you need a specific precision
/// specify it explicitly. If unsure, use the default. If you need a time point
/// on a clock other than the system_clock, use std::chrono directly.
template <typename D = std::chrono::nanoseconds>
using TimePoint = StdTime<SystemClock, D>;

#if defined(__clang__)
/// For some stupid reason, `chrono::utc_clock` isn't implemented on Clang.
/// Because of this, we need to provide our own definition. Yay!!
class UtcClock : public SystemClock {};
#else
/// This exists on every other compiler!!!
using UtcClock = std::chrono::utc_clock;
#endif

template <typename D = std::chrono::nanoseconds>
using UtcTime = StdTime<UtcClock, D>;

/// Convert a std::time_t to a UtcTime
inline auto toUtcTime(std::time_t T) -> UtcTime<std::chrono::seconds> {
  using namespace std::chrono;
  return UtcTime<seconds>(seconds(T));
}

/// Convert a TimePoint to std::time_t
inline std::time_t toTimeT(TimePoint<> TP) {
  using namespace std::chrono;
  return SystemClock::to_time_t(
      time_point_cast<SystemClock::time_point::duration>(TP));
}

/// Convert a UtcTime to std::time_t
inline std::time_t toTimeT(UtcTime<> TP) {
  using namespace std::chrono;
  return SystemClock::to_time_t(time_point<SystemClock, seconds>(
      duration_cast<seconds>(TP.time_since_epoch())));
}

/// Convert a std::time_t to a TimePoint
inline auto toTimePoint(std::time_t T) -> TimePoint<std::chrono::seconds> {
  using namespace std::chrono;
  return time_point_cast<seconds>(SystemClock::from_time_t(T));
}

/// Convert a std::time_t + nanoseconds to a TimePoint
inline TimePoint<> toTimePoint(std::time_t T, uint32_t nsec) {
  using namespace std::chrono;
  return time_point_cast<nanoseconds>(SystemClock::from_time_t(T))
    + nanoseconds(nsec);
}

/// Get the current time as a `TimePoint<>`.
inline TimePoint<> now() {
  return SystemClock::now();
}

} // namespace sys

//======================================================================//
// Printing
//======================================================================//

namespace H {

template <typename Period> struct unit { static const char value[]; };
template <typename Period> const char unit<Period>::value[] = "";

template <> struct unit<std::ratio<3600>> { static const char value[]; };
template <> struct unit<std::ratio<60>> { static const char value[]; };
template <> struct unit<std::ratio<1>> { static const char value[]; };
template <> struct unit<std::milli> { static const char value[]; };
template <> struct unit<std::micro> { static const char value[]; };
template <> struct unit<std::nano> { static const char value[]; };

raw_ostream& print_duration(raw_ostream& OS, intmax_t V, const char* Unit);
raw_ostream& print_duration(raw_ostream& OS, double D, const char* Unit);

} // namespace H

//////////////////////////////////////////////////////////////////////////
// Helper

template <typename Rep, typename Period>
struct TimePointUtil<std::chrono::duration<Rep, Period>> {
  using Dur = std::chrono::duration<Rep, Period>;
  using InternalRep = H::TimePointRep<Rep>;
public:
  template <typename AsPeriod = Period>
  static InternalRep GetAs(const Dur& D) {
    using namespace std::chrono;
    return duration_cast<duration<InternalRep, AsPeriod>>(D).count();
  }

  static const char* GetUnitRaw() {
    return {H::unit<Period>::value};
  }
};

//////////////////////////////////////////////////////////////////////////
// Implementation

raw_ostream& operator<<(raw_ostream& OS, const sys::TimePoint<>& D);
raw_ostream& operator<<(raw_ostream& OS, const sys::UtcTime<>& D); // TODO <<

template <typename Rep, typename Dur>
raw_ostream& operator<<(raw_ostream& OS,
 const std::chrono::duration<Rep, Dur>& D) {
  using Ty = std::chrono::duration<Rep, Dur>;
  constexpr TimePointUtil<Ty> U {};
  return H::print_duration(OS, U.GetAs(D), U.GetUnitRaw());
}

} // namespace exi
