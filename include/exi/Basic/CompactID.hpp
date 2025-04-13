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

template <bool NeverZero = false>
EXI_INLINE constexpr u32 CompactIDLog2(CompactID ID) noexcept {
  if constexpr (NeverZero)
    exi_invariant(ID > 0);
  else {
    if EXI_UNLIKELY(ID == 0)
      return 0;
  }
  // Same as Log2_64 for now, may change in the future.
  return 63 - std::countl_zero(ID);
}

namespace H {

// TODO: Add embedded counters?

template <typename T>
struct CompactIDBaseHolder {
  static_assert(!std::is_abstract_v<T>);
  using type = T;
};

template <typename T>
requires (!std::is_class_v<T> || std::is_final_v<T>)
struct CompactIDBaseHolder<T> {
  static_assert(!std::is_abstract_v<T>);
public:
  /// Use self as the type.
  using type = CompactIDBaseHolder<T>;
private:
  T Data;
};

} // namespace H

template <u64 Offset = 0>
class CompactIDCounter {
  CompactID Value = 0;
  u32 LogValue = 0;

  ALWAYS_INLINE constexpr u32 Log2(CompactID ID) noexcept {
    return CompactIDLog2<Offset != 0>(ID);
  }

  /// Runs the compact log2 calculation on the current value. 
  EXI_INLINE constexpr void recalculateLog() {
    LogValue = Log2(this->Value + Offset);
  }

public:
  /// Starts counter from 0.
  EXI_INLINE constexpr CompactIDCounter() = default;
  /// Starts counter from `StartingID`.
  constexpr CompactIDCounter(CompactID StartingID) :
   Value(StartingID), LogValue(Log2(StartingID)) {}
  
  /// Returns the current value of the counter.
  EXI_INLINE constexpr CompactID value() const { return Value; }
  /// Returns the minimum bits required for current value of the counter.
  EXI_INLINE constexpr u32 bits() const { return LogValue; }
  /// Returns the minimum bytes required for current value of the counter.
  EXI_INLINE constexpr u32 bytes() const {
    if EXI_UNLIKELY(Value == 0)
      return 0;
    return (LogValue / 8) + 1u;
  }

  /// Returns the current value of the counter.
  EXI_INLINE constexpr CompactID operator*() const { return Value; }

  /// Increments the counter by 1.
  constexpr void inc() {
    Value += 1;
    recalculateLog();
  }
  /// Increments the counter by `I`.
  constexpr void add(CompactID I) {
    Value += I;
    recalculateLog();
  }

  EXI_INLINE constexpr CompactIDCounter& operator++() {
    this->inc();
    return *this;
  }
  EXI_INLINE constexpr CompactIDCounter operator++(int) {
    CompactIDCounter Out = *this;
    this->inc();
    return Out;
  }

  /// Directly the value of the counter, avoid use if possible.
  constexpr void set(CompactID ID) {
    Value = ID;
    recalculateLog();
  }
};

} // namespace exi
