//===- exi/Basic/CompactID.hpp --------------------------------------===//
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

/// The "real" return type of log2 functions.
template <typename Int>
using cmlog2_int_t = std::conditional_t<
  sizeof(Int) >= 4, unsigned, std::make_unsigned_t<Int>>;

namespace H {

template <std::unsigned_integral UInt>
ALWAYS_INLINE constexpr cmlog2_int_t<UInt> CmLog2Dispatch(UInt ID) {
  constexpr_static unsigned kSubtract = bitsizeof_v<UInt>;
  return kSubtract - std::countl_zero(UInt(ID - 1u));
}

template <std::signed_integral Int>
[[deprecated("likely an accidental signed input, "
  "ensure small types are handled correctly!")]]
EXI_INLINE constexpr cmlog2_int_t<Int> CmLog2Dispatch(Int ID) {
  // TODO: Fix in permissive mode
  COMPILE_FAILURE(Int, "Signed inputs are not allowed!");
  using UInt = std::make_unsigned_t<Int>;
  exi_assume(ID >= 0);
  return CmLog2Dispatch(static_cast<UInt>(ID));
}

} // namespace H

/// Calculates `⌈ log2(ID) ⌉`.
/// @note Call `Log2_N_Ceil` directly if exact size is required.
/// @tparam NeverZero Whether the zero case need be considered.
template <bool NeverZero = false, std::integral Int>
constexpr auto ID_Log2(Int ID) {
  // Faster algorithm.
  if constexpr (NeverZero) {
    exi_invariant(ID > 0);
    return H::CmLog2Dispatch(ID);
  } else {
    return EXI_LIKELY(ID != 0u)
      ? H::CmLog2Dispatch(ID)
      : cmlog2_int_t<Int>(0u);
  }
}

/// Calculates `⌈ log2(ID) ⌉`.
/// @note Call `Log2_N_Ceil` directly if exact size is required.
/// @tparam Offset The offset of logarithm inputs.
template <u64 Offset, std::integral Int>
EXI_INLINE constexpr auto ID_OffsetLog2(Int ID) {
  exi_invariant(ID >= Offset);
  return ID_Log2<Offset != 0, Int>(ID);
}

/// Calculates `⌈ log2(ID + Offset) ⌉`.
/// @warning THIS WILL ADD THE OFFSET! Use `ID_OffsetLog2` for normal checks.
/// @tparam Offset The offset used on the inputs.
template <u64 Offset, std::integral Int>
EXI_INLINE constexpr auto ID_AddOffsetLog2(Int ID) {
  return ID_Log2<Offset != 0, Int>(ID + Int(Offset));
}

template <class> class IntrusiveCounterHandle;
template <typename, typename, u64> class IDCounterHandle;

/// A counter with the LogValue embedded.
/// TODO: Check if a countdown would make this more efficient...
template <std::unsigned_integral T, u64 Offset = 0>
class IDLogCounter {
  template <class> friend class IntrusiveCounterHandle;
  template <typename, typename, u64> friend class IDCounterHandle;
  using LogT = cmlog2_int_t<T>;
  T Value = 0;
  LogT LogValue = 0;

  ALWAYS_INLINE constexpr LogT Log2(T ID) {
    return ID_AddOffsetLog2<Offset>(ID);
  }

  /// Runs the compact log2 calculation on the current value. 
  EXI_INLINE constexpr void recalculateLog() {
    LogValue = Log2(this->Value);
  }

public:
  /// Starts counter from 0.
  EXI_INLINE constexpr IDLogCounter() = default;
  /// Starts counter from `StartingID`.
  constexpr IDLogCounter(T StartingID) :
   Value(StartingID), LogValue(Log2(StartingID)) {}
  
  /// Returns the current value of the counter.
  EXI_INLINE constexpr T value() const { return Value; }
  /// Returns the minimum bits required for current value of the counter.
  EXI_INLINE constexpr unsigned bits() const { return LogValue; }
  /// Returns the minimum bytes required for current value of the counter.
  EXI_INLINE constexpr unsigned bytes() const {
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

  EXI_INLINE constexpr IDLogCounter& operator++() {
    this->inc();
    return *this;
  }
  EXI_INLINE constexpr IDLogCounter operator++(int) {
    IDLogCounter Out = *this;
    this->inc();
    return Out;
  }

  /// Directly the value of the counter, avoid use if possible.
  constexpr void set(T ID) {
    Value = ID;
    recalculateLog();
  }
};

// An IDLogCounter for `CompactID`s.
template <u64 Offset = 0>
using CompactIDCounter = IDLogCounter<CompactID, Offset>;

/// A container wrapper which embeds a log counter based on `.size()`.
template <class Clazz, u64 Offset = 0>
class IntrusiveLogCounter {
  template <class> friend class IntrusiveCounterHandle;
  Clazz Data;
  u32 LogValue = 0;

  ALWAYS_INLINE constexpr u32 Log2(auto ID) {
    return ID_AddOffsetLog2<Offset>(ID);
  }

public:
  constexpr IntrusiveLogCounter(auto&&...Args) :
   Data(EXI_FWD(Args)...), LogValue(Log2(Data.size())) {}
  
  /// Returns the minimum bits required for current value of the counter.
  EXI_INLINE constexpr unsigned bits() const { return LogValue; }
  /// Returns the minimum bytes required for current value of the counter.
  EXI_INLINE constexpr unsigned bytes() const {
    if EXI_UNLIKELY(Data.size() == 0)
      return 0;
    return (LogValue / 8) + 1u;
  }

  /// Runs the compact log2 calculation on the current value. 
  EXI_INLINE constexpr void recalculateLog() {
    LogValue = Log2(Data.size());
  }

  constexpr Clazz& value() { return Data; }
  constexpr const Clazz& value() const { return Data; }

  constexpr Clazz& operator*() { return Data; }
  constexpr const Clazz& operator*() const { return Data; }

  constexpr Clazz* operator->() { return &Data; }
  constexpr const Clazz* operator->() const { return &Data; }
};

/// An RTTI handle that updates the log at the end of the scope.
template <class Counter> class IntrusiveCounterHandle {
  static_assert(!std::is_const_v<Counter>);
  Counter& Data;
public:
  // TODO: Mark pinned?
  IntrusiveCounterHandle(Counter& Data EXI_LIFETIMEBOUND) : Data(Data) {}
  ~IntrusiveCounterHandle() { Data.recalculateLog(); }
  Counter* operator->() { return &Data; }
};

/// An RTTI handle that updates the log at the end of the scope.
/// Assumes the counter starts at 0 unless otherwise specified.
template <typename Int, typename LogT, u64 Offset = 0>
class IDCounterHandle {
  static_assert(!std::is_const_v<LogT>);
  const Int& Data;
  LogT& Log;
public:
  // TODO: Mark pinned?
  IDCounterHandle(Int&&, LogT& Log) = delete;
  IDCounterHandle(const Int& Data EXI_LIFETIMEBOUND, LogT& Log)
      : Data(Data), Log(Log) {}
  explicit IDCounterHandle(IDLogCounter<Int, Offset>& Val)
      : Data(Val.Value), Log(Val.LogValue) {}
  ~IDCounterHandle() { Log = ID_AddOffsetLog2<Offset, Int>(Data); }
};

template <u64 Offset, typename T, typename LogT>
inline auto make_idhandle(const T& Data EXI_LIFETIMEBOUND, LogT& Log)
 -> IDCounterHandle<T, LogT, Offset> {
  return IDCounterHandle<T, LogT, Offset>(Data, Log);
}

template <typename T, u64 Offset>
inline auto make_idhandle(IDLogCounter<T, Offset>& Val)
 -> IDCounterHandle<T, cmlog2_int_t<T>, Offset> {
  return IDCounterHandle<T, cmlog2_int_t<T>, Offset>(Val);
}

template <class Counter>
IntrusiveCounterHandle(Counter&)
  -> IntrusiveCounterHandle<Counter>;

template <typename T, typename LogT>
IDCounterHandle(T&, LogT&)
  -> IDCounterHandle<std::remove_const_t<T>, LogT>;

template <typename T, u64 Offset>
IDCounterHandle(IDLogCounter<T, Offset>&)
  -> IDCounterHandle<T, cmlog2_int_t<T>, Offset>;

} // namespace exi
