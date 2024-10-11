//===- exip/config.h ------------------------------------------------===//
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

#ifndef EXIPCONFIG2_H_
#define EXIPCONFIG2_H_

#include "exipConfig.h"

#if EXIP_SCOPE_NS
# define EXIP_NS_TAG
#else
# define EXIP_NS_TAG inline
#endif

#ifdef __cplusplus
# define EXIP_BEGIN_DEFS \
  EXIP_NS_TAG namespace exip { \
    extern "C" {
# define EXIP_END_DEFS \
    } \
  }
#else
# define EXIP_BEGIN_DEFS
# define EXIP_END_DEFS
#endif

#if defined(__cplusplus) || (__STDC_VERSION__ >= 199901L)
# define EXIP_FUNC __func__
#elif defined(__STDC__) && defined(__GNUC__)
# define EXIP_FUNC __FUNCTION__
#else
# define EXIP_FUNC NULL
# define EXIP_FUNC_NONE 1
#endif

#ifdef __GNUC__
# define EXIP_EXPECT(v, expr) (__builtin_expect(!!(expr), v))
#else
# define EXIP_EXPECT(v, expr) (!!(expr))
#endif

#define EXIP_LIKELY(...)   EXIP_EXPECT(1, (__VA_ARGS__))
#define EXIP_UNLIKELY(...) EXIP_EXPECT(0, (__VA_ARGS__))

#ifdef __GNUC__
# define EXIP_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
# define EXIP_ALWAYS_INLINE inline
#endif

#endif // EXIPCONFIG2_H_
