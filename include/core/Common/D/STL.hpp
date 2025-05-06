//===- Common/D/STL.hpp ---------------------------------------------===//
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

#pragma once

#if __has_include(<__config>) && defined(__clang__)
# include <__config>
# define EXI_STL_LIBCPP 1
#elif __has_include(<yvals_core.h>) && (defined(_MSC_VER) && !defined(__GNUC__))
# include <yvals_core.h>
# define EXI_STL_MSVC 1
#elif __has_include(<bits/c++config.h>)
# include <bits/c++config.h>
# define EXI_STL_GLIBCXX 1
#endif

#if __has_include(<sys/cdefs.h>)
# include <sys/cdefs.h>
#endif

#if EXI_STL_GLIBCXX && defined(__STRICT_ANSI__)
# define EXI_STL_GLIBC_STRICTANSI
#endif
