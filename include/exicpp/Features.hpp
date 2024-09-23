//===- exicpp/Features.hpp ------------------------------------------===//
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
//
//  This file acts as a source for in-language configuration.
//
//===----------------------------------------------------------------===//

#pragma once

#ifndef EXIP_FEATURES_HPP
#define EXIP_FEATURES_HPP

#include <exipConfig.h>

#undef EXICPP_MSVC
#if (defined(_MSC_VER) || defined(_MSVC_LANG)) && !defined(__MINGW32__)
# define EXICPP_MSVC 1
#endif

#if defined(_MSC_VER) || defined(_MSVC_LANG)
/// The C++ language version on MSVC.
# define EXICPP_LANG _MSVC_LANG
#elif __cplusplus
/// The C++ language version.
# define EXICPP_LANG __cplusplus
#else
# error exiCPP must be compiled with a C++ compiler!
#endif

#ifdef __has_builtin
# define EXICPP_HAS_BUILTIN(x) __has_builtin(x)
#else
# define EXICPP_HAS_BUILTIN(x) 0
#endif

#ifdef __has_attribute
# define EXICPP_HAS_ATTR(x) __has_attribute(x)
#else
# define EXICPP_HAS_ATTR(x) 0
#endif

#ifdef __has_cpp_attribute
# define EXICPP_HAS_CPPATTR(x) __has_cpp_attribute(x)
#else
# define EXICPP_HAS_CPPATTR(x) 0
#endif

//======================================================================//
// Compiler Specific
//======================================================================//

#ifndef ALWAYS_INLINE
# if EXICPP_HAS_ATTR(always_inline)
#  define ALWAYS_INLINE __attribute__((always_inline)) inline
# elif EXICPP_HAS_ATTR(__always_inline__)
#  define ALWAYS_INLINE __attribute__((__always_inline__)) inline
# elif EXICPP_MSVC
#  define ALWAYS_INLINE __forceinline
# else
#  define ALWAYS_INLINE inline
# endif
#endif // ALWAYS_INLINE

#ifdef __GNUC__
# define EXICPP_FUNCTION __PRETTY_FUNCTION__
#elif EXICPP_MSVC
# define EXICPP_FUNCTION __FUNCSIG__
#else
# define EXICPP_FUNCTION __func__
#endif // EXICPP_FUNCTION

#if EXICPP_HAS_CPPATTR(likely)
# define EXICPP_LIKELY(...)   (__VA_ARGS__) [[likely]]
# define EXICPP_UNLIKELY(...) (__VA_ARGS__) [[unlikely]]
#elif EXICPP_HAS_BUILTIN(__builtin_expect)
# define EXICPP_EXPECT(v, expr) \
 __builtin_expect(static_cast<bool>(__VA_ARGS__), v)
# define EXICPP_LIKELY(...)   (EXICPP_EXPECT(1, (__VA_ARGS__)))
# define EXICPP_UNLIKELY(...) (EXICPP_EXPECT(0, (__VA_ARGS__)))
#else
# define EXICPP_LIKELY(...)   (__VA_ARGS__)
# define EXICPP_UNLIKELY(...) (__VA_ARGS__)
#endif

//======================================================================//
// Language Version
//======================================================================//

#if __cpp_concepts >= 201907L
# define EXICPP_CONCEPTS 1
#endif

#endif // EXIP_FEATURES_HPP
