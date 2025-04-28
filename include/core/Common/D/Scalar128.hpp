//===- Common/D/Scalar128.hpp ---------------------------------------===//
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
/// This file defines flags for 128-bit scalars.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Common/D/STL.hpp>

// __int128 check
#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
# define EXI_HAS_I128 1
#elif !EXI_STL_GLIBC_STRICTANSI && defined(__GLIBCXX_TYPE_INT_N_0)
// GCC supports __int128
# define EXI_HAS_I128 1
#elif EXI_STL_LIBCPP && !defined(_LIBCPP_HAS_NO_INT128)
// Clang supports __int128
# define EXI_HAS_I128 1
#else
# define EXI_HAS_I128 0
#endif

// __float128 check
#if defined(__clang__) && (defined(_M_X64) || defined(__x86_64__))
// Clang supports __float128
# define EXI_HAS_F128 1
#elif !EXI_STL_GLIBC_STRICTANSI && defined(_GLIBCXX_USE_FLOAT128)
// GCC supports __float128
# define EXI_HAS_F128 1
#else
# define EXI_HAS_F128 0
#endif
