//===- Common/ScopeExit.hpp -----------------------------------------===//
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
/// This file defines the make_scope_exit function, which executes user-defined
/// cleanup logic at scope exit.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/Features.hpp>
#include <type_traits>
#include <utility>

namespace exi {
namespace H {

template <typename Callable> class ScopeExit {
  Callable ExitFunction;
  bool Engaged = true; // False once moved-from or release()d.

public:
  template <typename Fp>
  explicit ScopeExit(Fp &&F) : ExitFunction(std::forward<Fp>(F)) {}

  ScopeExit(ScopeExit &&Rhs) : 
   ExitFunction(std::move(Rhs.ExitFunction)), Engaged(Rhs.Engaged) {
    Rhs.release();
  }
  ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(ScopeExit &&) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;

  void release() { Engaged = false; }

  ~ScopeExit() {
    if (Engaged) {
      ExitFunction();
    }
  }
};

} // namespace H

// Keeps the callable object that is passed in, and execute it at the
// destruction of the returned object (usually at the scope exit where the
// returned object is kept).
//
// Interface is specified by p0052r2.
template <typename Callable>
[[nodiscard]] H::ScopeExit<std::decay_t<Callable>>
 make_scope_exit(Callable &&F) {
  return H::ScopeExit<std::decay_t<Callable>>(std::forward<Callable>(F));
}

} // namespace llvm
