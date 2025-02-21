//===- Support/BuryPointer.hpp --------------------------------------===//
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

#include <Common/Box.hpp>

namespace exi {

// In tools that will exit soon anyway, going through the process of explicitly
// deallocating resources can be unnecessary - better to leak the resources and
// let the OS clean them up when the process ends. Use this function to ensure
// the memory is not misdiagnosed as an unintentional leak by leak detection
// tools (this is achieved by preserving pointers to the object in a globally
// visible array).
void BuryPointer(const void* Ptr);
template <typename T> void BuryPointer(Box<T> Ptr) {
  BuryPointer(Ptr.release());
}

} // namespace exi
