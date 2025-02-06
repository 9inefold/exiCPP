//===- Support/float128.hpp -----------------------------------------===//
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

#pragma once

#include <Config/Config.inc>

namespace exi {

#if defined(__clang__) && defined(__FLOAT128__) &&                             \
    defined(__SIZEOF_INT128__) && !defined(__LONG_DOUBLE_IBM128__)
#define HAS_IEE754_FLOAT128 1
typedef __float128 float128;
#elif defined(__FLOAT128__) && defined(__SIZEOF_INT128__) &&                   \
    !defined(__LONG_DOUBLE_IBM128__) &&                                        \
    (defined(__GNUC__) || defined(__GNUG__))
#define HAS_IEE754_FLOAT128 1
typedef _Float128 float128;
#endif

} // namespace exi
