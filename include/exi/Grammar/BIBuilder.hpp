//===- exi/Grammar/BIBuilder.hpp ------------------------------------===//
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
/// This file provides a type which can build event codes for builtin schemas.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Support/Logging.hpp>
#include <exi/Basic/EventCodes.hpp>
#include <exi/Basic/ExiOptions.hpp>
#include <exi/Grammar/BITypes.hpp>

#define DEBUG_TYPE "BuiltinSchema"

namespace exi {

/// A small log2 table for deducing bit counts. The maximum value a builtin
/// schema can have is 7, with `StartTagContent.{CM, PI}` with `SC` enabled.
alignas(16) inline constexpr u8 BISmallLog2[10] {0, 0, 1, 2, 2, 3, 3, 3, 3, 4};

/// The transitions for schemaless encodings are defined as the following.
/// If SC is not enabled, then the ChildContentItems for StartTagContent will
/// be (0.3) instead.
///
/// Document:
///   SD DocContent           0
/// 
/// DocContent:
///   SE (*) DocEnd           0
///   DT DocContent           1.0
///   CM DocContent           1.1.0
///   PI DocContent           1.1.1
/// 
/// DocEnd:
///   ED                      0
///   CM DocEnd               1.0
///   PI DocEnd               1.1
/// 
/// StartTagContent:
///   EE                      0.0
///   AT (*) StartTagContent  0.1
///   NS StartTagContent      0.2
///   SC Fragment             0.3
///   ChildContentItems      (0.4)  
/// 
/// ElementContent:
///   EE                      0
///   ChildContentItems      (1.0)  
/// 
/// ChildContentItems (n.m):
///   SE (*) ElementContent  n. m
///   CH ElementContent      n.(m+1)
///   ER ElementContent      n.(m+2)
///   CM ElementContent      n.(m+3).0
///   PI ElementContent      n.(m+3).1

/// Used to build the `TrailingArray`s for encoder/decoder schemas.
class BIBuilder {
  SmallVec<EventTerm, 8> Terms;
  SmallVec<BIInfo, BIInfoArray::size()> Info;
  ExiOptions::PreserveOpts Preserve;
  bool SelfContained;

  /// Handles calculations at the end of scope.
  class EventCodeRTTI {
    SEventCode* C;
  public:
    EventCodeRTTI(SEventCode* EC) : C(EC) {}
    ~EventCodeRTTI() { BIBuilder::CalculateLog(C); }
    SEventCode& operator*() { return *C; }
    SEventCode* operator->() { return C; }
  };

  inline static void CalculateLog(SEventCode* EC);

  EventCodeRTTI createBIInfo() {
    const u8 Offset = IntCast<u8>(Terms.size());
    Info.push_back({
      .Offset = Offset,
      .Code { .Length = 1 }
    });
    return &Info.back().Code;
  }

public:
  // TODO: Add error argument?
  BIBuilder(const ExiOptions& Opts) :
   Preserve(Opts.Preserve),
   SelfContained(Opts.SelfContained) {
    this->init();
  }

  static BIBuilder New(const ExiOptions& Opts) {
    return BIBuilder(Opts);
  }

  static void Inc(SEventCode& C, i8 I = 1) {
    if EXI_LIKELY(C.Length)
      C.Data[C.Length - 1] += I;
    else
      LOG_WARN("'Inc' ran on empty EventCode.");
  }
  static void Next(SEventCode& C) {
    if EXI_LIKELY(C.Length < 3)
      ++C.Length;
    else
      LOG_WARN("'Next' ran on full EventCode.");
  }
  static void IncNext(SEventCode& C, i8 I = 1) {
    BIBuilder::Inc(C, I);
    BIBuilder::Next(C);
  }

  ArrayRef<EventTerm> terms() const { return Terms; }
  ArrayRef<BIInfo> info() const { return Info; }
  /// @returns `Terms.size()`, used to initialize `TrailingArray`.
  unsigned trailing() const { return Terms.size(); }

  inline explicit operator bool() const {
    return !Terms.empty() && !Info.empty();
  }

private:
  void init() {
    /*DocContent:*/ {
      LOG_EXTRA("DocContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::SE);
      BIBuilder::Inc(*C);

      if (Preserve.DTDs) {
        Terms.push_back(EventTerm::DT);
        BIBuilder::IncNext(*C);
        BIBuilder::Inc(*C);
      }

      this->addCMPI(*C);
    }

    /*DocEnd:*/ {
      LOG_EXTRA("DocEnd:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::ED);
      BIBuilder::Inc(*C);
      this->addCMPI(*C);
    }

    /*StartTagContent:*/ {
      LOG_EXTRA("StartTagContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::EE);
      Terms.push_back(EventTerm::AT);
      BIBuilder::Next(*C);
      BIBuilder::Inc(*C, 2);

      if (Preserve.Prefixes) {
        Terms.push_back(EventTerm::NS);
        BIBuilder::Inc(*C);
      }

      if (SelfContained) {
        Terms.push_back(EventTerm::SC);
        BIBuilder::Inc(*C);
      }

      this->addCCItems(*C);
    }

    /*ElementContent:*/ {
      LOG_EXTRA("ElementContent:");
      auto C = createBIInfo();
      Terms.push_back(EventTerm::EE);
      BIBuilder::Inc(*C, 2);
      this->addCCItems(*C);
    }
  }

  /// Adds CM/PI to the end of a grammar, if possible.
  void addCMPI(SEventCode& C) {
    if (!Preserve.Comments && !Preserve.PIs)
      return;
    BIBuilder::IncNext(C);
    if (Preserve.Comments) {
      Terms.push_back(EventTerm::CM);
      BIBuilder::Inc(C);
    }
    if (Preserve.PIs) {
      Terms.push_back(EventTerm::PI);
      BIBuilder::Inc(C);
    }
  }

  /// Adds ChildContentItems.
  void addCCItems(SEventCode& C) {
    exi_assert(C.Length <= 2);
    C.Length = 2; // Add to x.[y].~

    Terms.push_back(EventTerm::SE);
    Terms.push_back(EventTerm::CH);
    BIBuilder::Inc(C, 2);

    if (Preserve.DTDs) {
      Terms.push_back(EventTerm::ER);
      BIBuilder::Inc(C);
    }

    this->addCMPI(C);
  }
};

void BIBuilder::CalculateLog(SEventCode* EC) {
  exi_invariant(EC);
  exi_assert(EC->Length <= 3 && EC->Length >= 0);

  auto& Length = EC->Length;
  if (Length == 0)
    return;
  
  auto& Data = EC->Data;
  // If we have `[x.y.0]`, make it `[x.y].0`.
  if (Length == 3 && !Data[2]) {
    Length -= 1;
  }

  // If we have `[x.0.z]`, make it `[x.z].0`.
  if (Length >= 2 && !Data[1]) {
    Data[1] = Data[2];
    Data[2] = 0;
    Length -= 1;
  }

  // We can't remove the first level event code, as it's possible grammars will
  // extend the base values. This would lead to inaccuracies.

  // Calculate log for all elements.
  for (int Ix = 0; Ix < 3; ++Ix)
    EC->Bits[Ix] = BISmallLog2[Data[Ix]];
}

} // namespace exi

#undef DEBUG_TYPE
