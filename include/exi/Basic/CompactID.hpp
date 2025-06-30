//===- exi/Basic/CompactID.hpp --------------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines Compact IDentifier utilities used by the EXI processor.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Fundamental.hpp>
#include <core/Common/bit.hpp>
#include <core/Support/ErrorHandle.hpp>

namespace exi {

/// The Compact ID type.
using CompactID = u64;

namespace H {

ALWAYS_INLINE constexpr u32 CmLog2Dispatch(u64 ID) {
  return Log2_64(ID - 1ul) + 1u;
}

ALWAYS_INLINE constexpr u32 CmLog2Dispatch(u32 ID) {
  return Log2_32(ID - 1u) + 1u;
}

ALWAYS_INLINE constexpr u16 CmLog2Dispatch(u16 ID) {
  return std::countl_zero(u16(ID - 1u)) + 1u;
}

ALWAYS_INLINE constexpr u8 CmLog2Dispatch(u8 ID) {
  return std::countl_zero(u8(ID - 1u)) + 1u;
}

} // namespace H

/// Calculates `⌈ log2(ID) ⌉`.
template <bool NeverZero = false, std::integral Int>
EXI_INLINE constexpr auto CompactIDLog2(Int ID) {
  // Faster algorithm.
  if constexpr (NeverZero) {
    exi_invariant(ID > 0);
    return H::CmLog2Dispatch(ID);
  } else {
    return EXI_LIKELY(ID != 0)
      ? H::CmLog2Dispatch(ID) : 0u;
  }
}

template <class> class LogCounterHandle;

/// A counter with the LogValue embedded.
/// TODO: Check if a countdown would make this more efficient...
template <std::integral T, u64 Offset = 0>
class EmbeddedLogCounter {
  template <class> friend class LogCounterHandle;
  T Value = 0;
  u32 LogValue = 0;

  ALWAYS_INLINE constexpr u32 Log2(T ID) {
    return CompactIDLog2<Offset != 0>(ID);
  }

  /// Runs the compact log2 calculation on the current value. 
  EXI_INLINE constexpr void recalculateLog() {
    LogValue = Log2(this->Value + T(Offset));
  }

public:
  /// Starts counter from 0.
  EXI_INLINE constexpr EmbeddedLogCounter() = default;
  /// Starts counter from `StartingID`.
  constexpr EmbeddedLogCounter(T StartingID) :
   Value(StartingID), LogValue(Log2(StartingID)) {}
  
  /// Returns the current value of the counter.
  EXI_INLINE constexpr T value() const { return Value; }
  /// Returns the minimum bits required for current value of the counter.
  EXI_INLINE constexpr u32 bits() const { return LogValue; }
  /// Returns the minimum bytes required for current value of the counter.
  EXI_INLINE constexpr u32 bytes() const {
    if EXI_UNLIKELY(Value == 0)
      return 0;
    return (LogValue / 8) + 1u;
  }

  /// Returns the current value of the counter.
  EXI_INLINE constexpr T operator*() const { return Value; }

  /// Increments the counter by 1.
  constexpr void inc() {
    Value += 1;
    recalculateLog();
  }
  /// Increments the counter by `I`.
  constexpr void add(T I) {
    Value += I;
    recalculateLog();
  }

  EXI_INLINE constexpr EmbeddedLogCounter& operator++() {
    this->inc();
    return *this;
  }
  EXI_INLINE constexpr EmbeddedLogCounter operator++(int) {
    EmbeddedLogCounter Out = *this;
    this->inc();
    return Out;
  }

  /// Directly the value of the counter, avoid use if possible.
  constexpr void set(T ID) {
    Value = ID;
    recalculateLog();
  }
};

// An EmbeddedLogCounter for `CompactID`s.
template <u64 Offset = 0>
using CompactIDCounter = EmbeddedLogCounter<CompactID, Offset>;

/// A container wrapper which embeds a log counter based on `.size()`.
template <class T, u64 Offset = 0>
class EmbeddedClassCounter {
  template <class> friend class LogCounterHandle;
  T Data;
  u32 LogValue = 0;

  ALWAYS_INLINE constexpr u32 Log2(auto ID) {
    return CompactIDLog2<Offset != 0>(ID);
  }

  /// Runs the compact log2 calculation on the current value. 
  EXI_INLINE constexpr void recalculateLog() {
    using IDType = decltype(Data.size());
    LogValue = Log2(Data.size() + IDType(Offset));
  }

public:
  constexpr EmbeddedClassCounter(auto&&...Args) :
   Data(EXI_FWD(Args)...), LogValue(Log2(Data.size())) {}
  
  /// Returns the minimum bits required for current value of the counter.
  EXI_INLINE constexpr u32 bits() const { return LogValue; }
  /// Returns the minimum bytes required for current value of the counter.
  EXI_INLINE constexpr u32 bytes() const {
    if EXI_UNLIKELY(Data.size() == 0)
      return 0;
    return (LogValue / 8) + 1u;
  }

  constexpr T& value() { return Data; }
  constexpr const T& value() const { return Data; }

  constexpr T& operator*() { return Data; }
  constexpr const T& operator*() const { return Data; }

  constexpr T* operator->() { return &Data; }
  constexpr const T* operator->() const { return &Data; }
};

/// An RTTI handle that updates the log at the end of the scope.
template <class Counter> class LogCounterHandle {
  static_assert(!std::is_const_v<Counter>);
  Counter& Data;
public:
  ALWAYS_INLINE LogCounterHandle(Counter& Data EXI_LIFETIMEBOUND) : Data(Data) {}
  ALWAYS_INLINE ~LogCounterHandle() { Data.recalculateLog(); }
  ALWAYS_INLINE Counter* operator->() { return &Data; }
};

/// An RTTI handle that updates the log at the end of the scope.
/// Assumes the counter starts at 0.
template <typename T, typename LogT> class LogProxyHandle {
  static_assert(!std::is_const_v<LogT>);
  const T& Data;
  LogT& Log;
public:
  LogProxyHandle(const T& Data EXI_LIFETIMEBOUND,
                 LogT& Log EXI_LIFETIMEBOUND) : Data(Data), Log(Log) {}
  LogProxyHandle(T&&, LogT& Log) = delete;
  ~LogProxyHandle() { Log = CompactIDLog2</*NeverZero=*/false>(Data); }
};

template <class Counter>
LogCounterHandle(Counter&) -> LogCounterHandle<Counter>;

template <typename T, typename LogT>
LogProxyHandle(T&, LogT&) -> LogProxyHandle<std::remove_const_t<T>, LogT>;

} // namespace exi
