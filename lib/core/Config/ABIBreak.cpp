//===- Config/ABIBreak.cpp ------------------------------------------===//
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

#include <Config/ABIBreak.inc>
// TODO: Get this working
// #include <mimalloc-new-delete.h>

namespace exi::abi_detail {
#if EXI_INVARIANTS
char debugModeEnabled;
#else
char debugModeDisabled;
#endif // EXICPP_DEBUG

#if EXI_USE_MIMALLOC
char mimallocEnabled;
#else
char mimallocDisabled;
#endif // EXI_USE_MIMALLOC

#if EXI_USE_THREADS
char threadingEnabled;
#else
char threadingDisabled;
#endif // EXI_USE_MIMALLOC
} // namespace exi::abi_detail
