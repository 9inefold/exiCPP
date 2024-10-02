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

#define CAT1_(a) a
#define CAT2_(a, b) a ## b
#define CAT3_(a, b, c) a ## b ## c
#define CAT4_(a, b, c, d) a ## b ## c ## d

#define CAT1(a) CAT1_(a)
#define CAT2(a, b) CAT2_(a, b)
#define CAT3(a, b, c) CAT2_(a, b, c)
#define CAT4(a, b, c, d) CAT2_(a, b, c, d)

//======================================================================//
// Setup
//======================================================================//

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

#define EXICPP_CPP98 199711L
#define EXICPP_CPP11 201103L
#define EXICPP_CPP14 201402L
#define EXICPP_CPP17 201703L
#define EXICPP_CPP20 202002L
#define EXICPP_CPP23 202302L

#define EXICPP_CPP(year) \
  (EXICPP_LANG >= CAT2_(EXICPP_CPP, year))

//////////////////////////////////////////////////////////////////////////

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
// Compiler Type
//======================================================================//

#define COMPILER_UNK_   0
#define COMPILER_GCC_   1
#define COMPILER_LLVM_  2
#define COMPILER_CLANG_ 2
#define COMPILER_MSVC_  3

#if EXICPP_MSVC
# define EXICPP_COMPILER_ COMPILER_MSVC_
# define EXICPP_COMPILER_MSVC 1
#elif defined(__clang__)
# define EXICPP_COMPILER_ COMPILER_CLANG_
# define EXICPP_COMPILER_LLVM 1
# define EXICPP_COMPILER_CLANG 1
#elif defined(__GNUC__)
# define EXICPP_COMPILER_ COMPILER_GCC_
# define EXICPP_COMPILER_GCC 1
#else
# define EXICPP_COMPILER_ COMPILER_UNK_
# error Unknown compiler!
#endif

#define EXICPP_COMPILER(name) \
  (EXICPP_COMPILER_ == CAT3_(COMPILER_, name, _))

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

#if EXICPP_HAS_CPPATTR(clang::lifetimebound)
# define LIFETIMEBOUND [[clang::lifetimebound]]
#elif EXICPP_HAS_CPPATTR(_Clang::__lifetimebound__)
# define LIFETIMEBOUND [[_Clang::__lifetimebound__]]
#else
# define LIFETIMEBOUND
#endif

#if EXICPP_HAS_ATTR(__nodebug__)
# define EXICPP_NODEBUG __attribute__((__nodebug__))
#else
# define EXICPP_NODEBUG
#endif

#if EXICPP_HAS_ATTR(__preferred_name__)
# define PREFERRED_NAME(alias) __attribute__((__preferred_name__(alias)))
#else
# define PREFERRED_NAME(alias)
#endif

//////////////////////////////////////////////////////////////////////////

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

#if EXICPP_HAS_BUILTIN(__builtin_unreachable)
# define EXICPP_UNREACHABLE() __builtin_unreachable()
#elif EXICPP_HAS_BUILTIN(__builtin_assume)
# define EXICPP_UNREACHABLE() __builtin_assume(0)
#elif EXICPP_COMPILER_MSVC
# define EXICPP_UNREACHABLE() __assume(0)
#else
# if EXICPP_NO_UNREACHABLE_FALLBACK
#  define EXICPP_UNREACHABLE() ((void)0)
# else
#  include <cassert>
#  define EXICPP_UNREACHABLE() assert(0)
# endif
#endif

//======================================================================//
// Language Version
//======================================================================//

#if __cpp_concepts >= 201907L
# define EXICPP_CONCEPTS 1
#endif

#define EXICPP_CXPR11 constexpr

#if EXICPP_CPP(14)
# define EXICPP_CXPR14 constexpr
#else
/// Constexpr in C++14.
# define EXICPP_CXPR14
#endif

#if EXICPP_CPP(17)
# define EXICPP_CXPR17 constexpr
#else
/// Constexpr in C++17.
# define EXICPP_CXPR17
#endif

#if EXICPP_CPP(20)
# define EXICPP_CXPR20 constexpr
#else
/// Constexpr in C++20.
# define EXICPP_CXPR20
#endif

#endif // EXIP_FEATURES_HPP
