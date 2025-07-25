//===- exi/Basic/D/InternalMacros.hpp -------------------------------===//
//
// Copyright (C) 2025 Eightfold
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
/// This file defines macros used by the implementation.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Features.hpp>

#ifdef __GNUC__
# define GNU_ATTR(...) __attribute__((__VA_ARGS__))
#else
# define GNU_ATTR(...)
#endif

#if EXI_HAS_ATTR(preserve_none)
/// Preserves none.
# define CC __attribute__((preserve_none))
/// Preserves none, inlines the function when unavailable.
# define CC_INLINE __attribute__((preserve_none))
#else
/// Empty attribute.
# define CC
/// Inlines the function.
# define CC_INLINE ALWAYS_INLINE
#endif

#ifdef __clang__
/// Keep debug information clean when using clang.
# define INTERNAL_LINKAGE [[clang::internal_linkage]]
# define INTERNAL_NS(NAME) NAME
# define HAS_INTERNAL_LINKAGE 1
#else
# define INTERNAL_LINKAGE
# define INTERNAL_NS(NAME)
# define HAS_INTERNAL_LINKAGE 0
#endif
