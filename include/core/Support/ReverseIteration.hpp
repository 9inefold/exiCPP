//===- Support/ReverseIteration.hpp ---------------------------------===//
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
/// If enabled, all supported unordered containers would be iterated in
/// reverse order. This is useful for uncovering non-determinism caused by
/// iteration of unordered containers.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Config/ABIBreak.inc>
#include <Support/PointerLikeTraits.hpp>

namespace exi {

template <class T = void *>
bool shouldReverseIterate() {
#if EXI_ENABLE_REVERSE_ITERATION
  return H::IsPointerLike<T>::value;
#else
  return false;
#endif
}

} // namespace exi
