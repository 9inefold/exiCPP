//===- Support/ManagedStatic.cpp -------------------------------------===//
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
// This file defines the ManagedStatic class and the exi_shutdown() function.
//
//===----------------------------------------------------------------===//

#include <Support/ManagedStatic.hpp>
#include <Config/Config.hpp>
#include <Support/ErrorHandle.hpp>
#include <Support/Threading.hpp>
#include <mutex>

using namespace exi;

static const ManagedStaticBase *StaticList = nullptr;

static std::recursive_mutex *getManagedStaticMutex() {
  static std::recursive_mutex m;
  return &m;
}

void ManagedStaticBase::RegisterManagedStatic(void *(*Creator)(),
                                              void (*Deleter)(void*)) const {
  assert(Creator);
  if (exi_is_multithreaded()) {
    std::lock_guard<std::recursive_mutex> Lock(*getManagedStaticMutex());

    if (!Ptr.load(std::memory_order_relaxed)) {
      void *Tmp = Creator();

      Ptr.store(Tmp, std::memory_order_release);
      DeleterFn = Deleter;

      // Add to list of managed statics.
      Next = StaticList;
      StaticList = this;
    }
  } else {
    exi_assert(!Ptr && !DeleterFn && !Next,
               "Partially initialized ManagedStatic!?");
    Ptr = Creator();
    DeleterFn = Deleter;

    // Add to list of managed statics.
    Next = StaticList;
    StaticList = this;
  }
}

void ManagedStaticBase::destroy() const {
  exi_assert(DeleterFn, "ManagedStatic not initialized correctly!");
  exi_assert(StaticList == this,
             "Not destroyed in reverse order of construction?");
  // Unlink from list.
  StaticList = Next;
  Next = nullptr;

  // Destroy memory.
  DeleterFn(Ptr);

  // Cleanup.
  Ptr = nullptr;
  DeleterFn = nullptr;
}

/// exi_shutdown - Deallocate and destroy all ManagedStatic variables.
/// IMPORTANT: it's only safe to call exi_shutdown() in single thread,
/// without any other threads executing EXI APIs.
/// exi_shutdown() should be the last use of EXI APIs.
void exi::exi_shutdown() {
  while (StaticList) {
    StaticList->destroy();
  }
}
