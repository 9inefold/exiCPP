//===- Support/BuryPointer.cpp --------------------------------------===//
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
///
/// \file
/// This file provides a mechanism to "bury" pointers to avoid leak detection
/// for intentional leaking.
///
//===----------------------------------------------------------------===//

#include <Support/BuryPointer.hpp>
#include <atomic>

using namespace exi;

void exi::BuryPointer(const void* Ptr) {
  // This function may be called only a small fixed amount of times per each
  // invocation, otherwise we do actually have a leak which we want to report.
  // If this function is called more than kGraveyardMaxSize times, the pointers
  // will not be properly buried and a leak detector will report a leak, which
  // is what we want in such case.
  static const usize kGraveyardMaxSize = 16;
  EXI_USED static const void* Graveyard[kGraveyardMaxSize];
  static std::atomic<unsigned> GraveyardSize;
  unsigned Ix = GraveyardSize++;
  if (Ix >= kGraveyardMaxSize)
    return;
  Graveyard[Ix] = Ptr;
}
