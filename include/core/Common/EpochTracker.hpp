//===- Common/EpochTracker.hpp --------------------------------------===//
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
///
/// \file
/// This file defines the DebugEpochBase and DebugEpochBase::HandleBase classes.
/// These can be used to write iterators that are fail-fast when EXICPP is built
/// with asserts enabled.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Config/ABIBreak.inc>
#include <Common/Fundamental.hpp>

namespace exi {

#if (EXI_DEBUG && !EXI_FAST_DEBUG)
# define EXI_DEBUGEPOCHBASE_HANDLEBASE_EMPTYBASE

/// A base class for data structure classes wishing to make iterators
/// ("handles") pointing into themselves fail-fast.  When building without
/// asserts, this class is empty and does nothing.
///
/// DebugEpochBase does not by itself track handles pointing into itself.  The
/// expectation is that routines touching the handles will poll on
/// isHandleInSync at appropriate points to assert that the handle they're using
/// is still valid.
///
class DebugEpochBase {
  u64 Epoch = 0;

public:
  DebugEpochBase() = default;

  /// Calling incrementEpoch invalidates all handles pointing into the
  /// calling instance.
  void incrementEpoch() { ++Epoch; }

  /// The destructor calls incrementEpoch to make use-after-free bugs
  /// more likely to crash deterministically.
  ~DebugEpochBase() { incrementEpoch(); }

  /// A base class for iterator classes ("handles") that wish to poll for
  /// iterator invalidating modifications in the underlying data structure.
  /// When LLVM is built without asserts, this class is empty and does nothing.
  ///
  /// HandleBase does not track the parent data structure by itself.  It expects
  /// the routines modifying the data structure to call incrementEpoch when they
  /// make an iterator-invalidating modification.
  ///
  class HandleBase {
    const u64 *EpochAddress = nullptr;
    u64 EpochAtCreation = UINT64_MAX;

  public:
    HandleBase() = default;

    explicit HandleBase(const DebugEpochBase *Parent)
        : EpochAddress(&Parent->Epoch), EpochAtCreation(Parent->Epoch) {}

    /// Returns true if the DebugEpochBase this Handle is linked to has
    /// not called incrementEpoch on itself since the creation of this
    /// HandleBase instance.
    bool isHandleInSync() const { return *EpochAddress == EpochAtCreation; }

    /// Returns a pointer to the epoch word stored in the data structure
    /// this handle points into.  Can be used to check if two iterators point
    /// into the same data structure.
    const void *getEpochAddress() const { return EpochAddress; }
  };
};

#else

# ifdef _MSC_VER
#  define EXI_DEBUGEPOCHBASE_HANDLEBASE_EMPTYBASE __declspec(empty_bases)
# else
#  define EXI_DEBUGEPOCHBASE_HANDLEBASE_EMPTYBASE
# endif // _MSC_VER

class DebugEpochBase {
public:
  void incrementEpoch() {}

  class HandleBase {
  public:
    HandleBase() = default;
    explicit HandleBase(const DebugEpochBase *) {}
    bool isHandleInSync() const { return true; }
    const void *getEpochAddress() const { return nullptr; }
  };
};

#endif // EXI_DEBUG && !EXI_FAST_DEBUG

} // namespace exi
