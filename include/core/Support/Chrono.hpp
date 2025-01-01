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

template <typename To, typename From, typename>
struct CastInfo;

namespace sys {

/// A time point on the system clock. This is provided for two reasons:
/// - to insulate us against subtle differences in behavior to differences in
///   system clock precision (which is implementation-defined and differs
///   between platforms).
/// - to shorten the type name
/// The default precision is nanoseconds. If you need a specific precision
/// specify it explicitly. If unsure, use the default. If you need a time point
/// on a clock other than the system_clock, use std::chrono directly.
template <typename D = std::chrono::nanoseconds>
using TimePoint = std::chrono::time_point<std::chrono::system_clock, D>;

template <typename D = std::chrono::nanoseconds>
using UtcTime = std::chrono::time_point<std::chrono::utc_clock, D>;

/// Convert a std::time_t to a UtcTime
inline auto toUtcTime(std::time_t T) -> UtcTime<std::chrono::seconds> {
  using namespace std::chrono;
  return UtcTime<seconds>(seconds(T));
}

/// Convert a TimePoint to std::time_t
inline std::time_t toTimeT(TimePoint<> TP) {
  using namespace std::chrono;
  return system_clock::to_time_t(
      time_point_cast<system_clock::time_point::duration>(TP));
}

/// Convert a UtcTime to std::time_t
inline std::time_t toTimeT(UtcTime<> TP) {
  using namespace std::chrono;
  return system_clock::to_time_t(time_point<system_clock, seconds>(
      duration_cast<seconds>(TP.time_since_epoch())));
}

/// Convert a std::time_t to a TimePoint
inline auto toTimePoint(std::time_t T) -> TimePoint<std::chrono::seconds> {
  using namespace std::chrono;
  return time_point_cast<seconds>(system_clock::from_time_t(T));
}

/// Convert a std::time_t + nanoseconds to a TimePoint
inline TimePoint<> toTimePoint(std::time_t T, uint32_t nsec) {
  using namespace std::chrono;
  return time_point_cast<nanoseconds>(system_clock::from_time_t(T))
    + nanoseconds(nsec);
}

//////////////////////////////////////////////////////////////////////////
// DynTime

namespace H {

template <class Clock> struct IsSystemClock;
template <> struct IsSystemClock<std::chrono::system_clock> : std::true_type {};
template <> struct IsSystemClock<std::chrono::utc_clock> : std::false_type {};

} // namespace H

template <class Clock>
concept is_system_clock = H::IsSystemClock<Clock>::value;

struct DynTimePoint {
  template <typename, typename, typename>
  friend struct CastInfo;

  template <class R>
  constexpr DynTimePoint(const TimePoint<R>& TP) :
   Data(&TP), Ratio(toDynRatio(R{})), IsSystem(true) {
    static_assert(CheckRatio<R>(),
      "Don't use durations greater than a day!");
  }

  template <class R>
  constexpr DynTimePoint(const UtcTime<R>& TP) :
   Data(&TP), Ratio(toDynRatio(R{})), IsSystem(true) {
    static_assert(CheckRatio<R>(),
      "Don't use durations greater than a day!");
  }

public:
  template <class R> static constexpr bool CheckRatio() {
    return std::ratio_less_equal_v<R, std::ratio<3600>>;
  }

  template <typename T, class R>
  static constexpr DynRatio ToDynRatio(
   const std::chrono::duration<T, R>&) {
    return exi::toDynRatio(R{});
  }

  bool isSystemTime() const { return IsSystem; }

  bool isSameRatio(const exi::DynRatio& In) const {
    return In == this->Ratio;
  }

  template <class R> bool isSameRatio() const {
    constexpr exi::DynRatio In = ToDynRatio(R{});
    return this->isSameRatio(In);
  }

  const void* data() const {
    return Data;
  }

private:
  const void* Data = nullptr;
  exi::DynRatio Ratio;
  bool IsSystem;
};

} // namespace sys

template <class Ratio>
struct CastInfo<sys::TimePoint<Ratio>, sys::DynTimePoint, void> {
  using To = sys::TimePoint<Ratio>;
  using From = sys::DynTimePoint;
  using Self = CastInfo<To, From, void>;
  using CastReturnType = To;

  static inline bool isPossible(From &f) {
    return f.isSameRatio<Ratio>() && f.isSystemTime();
  }

  static inline const To& doCast(From &f) {
    return *static_cast<const To*>(f.data());
  }

  static inline CastReturnType castFailed() { return To{}; }

  static inline CastReturnType doCastIfPossible(From &f) {
    if (!Self::isPossible(f))
      return Self::castFailed();
    return Self::doCast(f);
  }
};

raw_ostream &operator<<(raw_ostream &OS, sys::DynTimePoint TP);

} // namespace exi
