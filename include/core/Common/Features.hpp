//===- Common/Features.hpp ------------------------------------------===//
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
/// This file acts as a source for in-language configuration.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Config/Config.inc>
#include <cassert>
#include <cstddef>
#ifdef _MSC_VER
# include <sal.h>
#endif

#define CAT1_(a) a
#define CAT2_(a, b) a ## b
#define CAT3_(a, b, c) a ## b ## c
#define CAT4_(a, b, c, d) a ## b ## c ## d

#define CAT1(a) CAT1_(a)
#define CAT2(a, b) CAT2_(a, b)
#define CAT3(a, b, c) CAT2_(a, b, c)
#define CAT4(a, b, c, d) CAT2_(a, b, c, d)

#define XSTRINGIFY(...) #__VA_ARGS__
#define STRINGIFY(...) XSTRINGIFY(__VA_ARGS__)

//======================================================================//
// Setup
//======================================================================//

#undef EXI_MSVC
#if (defined(_MSC_VER) || defined(_MSVC_LANG)) && !defined(__MINGW32__)
# define EXI_MSVC 1
#endif

#if defined(_MSC_VER) || defined(_MSVC_LANG)
/// The C++ language version on MSVC.
# define EXI_LANG _MSVC_LANG
#elif __cplusplus
/// The C++ language version.
# define EXI_LANG __cplusplus
#else
# error exiCPP must be compiled with a C++ compiler!
#endif

#define EXI_CPP98 199711L
#define EXI_CPP11 201103L
#define EXI_CPP14 201402L
#define EXI_CPP17 201703L
#define EXI_CPP20 202002L
#define EXI_CPP23 202302L

#define EXI_CPP(year) \
  (EXI_LANG >= CAT2_(EXI_CPP, year))

//////////////////////////////////////////////////////////////////////////

#ifdef __has_builtin
# define EXI_HAS_BUILTIN(x) __has_builtin(x)
#else
# define EXI_HAS_BUILTIN(x) 0
#endif

#ifdef __has_attribute
# define EXI_HAS_ATTR(x) __has_attribute(x)
#else
# define EXI_HAS_ATTR(x) 0
#endif

#ifdef __has_cpp_attribute
# define EXI_HAS_CPPATTR(x) __has_cpp_attribute(x)
#else
# define EXI_HAS_CPPATTR(x) 0
#endif

#ifdef __has_feature
# define EXI_HAS_FEATURE(x) __has_feature(x)
#else
# define EXI_HAS_FEATURE(x) 0
#endif

//////////////////////////////////////////////////////////////////////////

#if EXI_EXCEPTIONS
# define EXI_NOEXCEPT
#else
# define EXI_NOEXCEPT noexcept
#endif

#if EXI_STRICT_NODISCARD
# define EXI_NODISCARD [[nodiscard]]
#else
# define EXI_NODISCARD
#endif

//======================================================================//
// Compiler Type
//======================================================================//

#define COMPILER_UNK_   0
#define COMPILER_GCC_   1
#define COMPILER_CLANG_ 2
#define COMPILER_MSVC_  3
#define COMPILER_LLVM_  COMPILER_CLANG_

#if EXI_MSVC
# define EXI_COMPILER_ COMPILER_MSVC_
# define EXI_COMPILER_MSVC 1
#elif defined(__clang__)
# define EXI_COMPILER_ COMPILER_CLANG_
# define EXI_COMPILER_LLVM 1
# define EXI_COMPILER_CLANG 1
#elif defined(__GNUC__)
# define EXI_COMPILER_ COMPILER_GCC_
# define EXI_COMPILER_GCC 1
#else
# define EXI_COMPILER_ COMPILER_UNK_
# error Unknown compiler!
#endif

#define EXI_COMPILER(name) \
  (EXI_COMPILER_ == CAT3_(COMPILER_, name, _))

//======================================================================//
// Compiler Specific
//======================================================================//

#if EXI_COMPILER(MSVC)
# define EXI_PRAGMA(...) __pragma(__VA_ARGS__)
#else
# define EXI_PRAGMA(...) _Pragma(#__VA_ARGS__)
#endif

#if EXI_COMPILER(CLANG)
# define DIAGNOSTIC_PUSH() EXI_PRAGMA(clang diagnostic push)
# define DIAGNOSTIC_POP()  EXI_PRAGMA(clang diagnostic pop)
# define CLANG_DIAGNOSTIC(sv, name) EXI_PRAGMA(clang diagnostic sv name)
#elif EXI_COMPILER(GCC)
# define DIAGNOSTIC_PUSH() EXI_PRAGMA(GCC diagnostic push)
# define DIAGNOSTIC_POP()  EXI_PRAGMA(GCC diagnostic pop)
# define GCC_DIAGNOSTIC(sv, name) EXI_PRAGMA(GCC diagnostic sv name)
#elif EXI_COMPILER(MSVC)
# define DIAGNOSTIC_PUSH() EXI_PRAGMA(warning(push))
# define DIAGNOSTIC_POP()  EXI_PRAGMA(warning(pop))
# define MSVC_DIAGNOSTIC(sv, code) __pragma(warning(sv:code))
#else
# define DIAGNOSTIC_PUSH()
# define DIAGNOSTIC_POP()
#endif

#ifndef CLANG_DIAGNOSTIC
# define CLANG_DIAGNOSTIC(sv, name)
#endif

#ifndef GCC_DIAGNOSTIC
# define GCC_DIAGNOSTIC(sv, name)
#endif

#ifndef MSVC_DIAGNOSTIC
# define MSVC_DIAGNOSTIC(sv, code)
#endif

#define CLANG_IGNORED(name) CLANG_DIAGNOSTIC(ignored, name)
#define CLANG_WARNING(name) CLANG_DIAGNOSTIC(warning, name)
#define CLANG_ERROR(name)   CLANG_DIAGNOSTIC(error, name)

#define GCC_IGNORED(name)   GCC_DIAGNOSTIC(ignored, name)
#define GCC_WARNING(name)   GCC_DIAGNOSTIC(warning, name)
#define GCC_ERROR(name)     GCC_DIAGNOSTIC(error, name)

#define MSVC_IGNORED(code)  MSVC_DIAGNOSTIC(disabled, code)
#define MSVC_WARNING(code)  MSVC_DIAGNOSTIC(1, code)
#define MSVC_ERROR(code)    MSVC_DIAGNOSTIC(error, code)

//////////////////////////////////////////////////////////////////////////
// Attributes

#if EXI_HAS_ATTR(always_inline)
# define EXI_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif EXI_HAS_ATTR(__always_inline__)
# define EXI_ALWAYS_INLINE __attribute__((__always_inline__)) inline
#elif defined(_MSC_VER)
# define EXI_ALWAYS_INLINE __forceinline
#else
# define EXI_ALWAYS_INLINE inline
#endif

#undef ALWAYS_INLINE
#define ALWAYS_INLINE EXI_ALWAYS_INLINE

#ifdef NDEBUG
/// Always inlines functions on release builds.
# define EXI_INLINE EXI_ALWAYS_INLINE
#else
/// Default inline on debug builds.
# define EXI_INLINE inline
#endif

#if EXI_HAS_ATTR(noinline)
# define EXI_NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define EXI_NO_INLINE __declspec(noinline)
#else
# define EXI_NO_INLINE
#endif

#undef NO_INLINE
#define NO_INLINE EXI_NO_INLINE

#if !EXI_MSVC
# define EXI_EMPTY_BASES
# define EXI_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
# define EXI_EMPTY_BASES __declspec(empty_bases)
# define EXI_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#endif

#if EXI_HAS_ATTR(nodebug)
# define EXI_NODEBUG __attribute__((nodebug))
#elif EXI_HAS_ATTR(__nodebug__)
# define EXI_NODEBUG __attribute__((__nodebug__))
#else
# define EXI_NODEBUG
#endif

#if EXI_HAS_CPPATTR(clang::lifetimebound)
# define EXI_LIFETIMEBOUND [[clang::lifetimebound]]
#elif EXI_HAS_CPPATTR(msvc::lifetimebound)
# define EXI_LIFETIMEBOUND [[msvc::lifetimebound]]
#elif EXI_HAS_CPPATTR(lifetimebound)
# define EXI_LIFETIMEBOUND [[lifetimebound]]
#else
# define EXI_LIFETIMEBOUND
#endif

#if EXI_HAS_CPPATTR(gsl::Owner)
# define EXI_GSL_OWNER [[gsl::Owner]]
#else
# define EXI_GSL_OWNER
#endif

#if EXI_HAS_CPPATTR(gsl::Pointer)
# define EXI_GSL_POINTER [[gsl::Pointer]]
#else
# define EXI_GSL_POINTER
#endif

#if EXI_HAS_ATTR(__nodebug__)
# define EXI_NODEBUG __attribute__((__nodebug__))
#else
# define EXI_NODEBUG
#endif

#if EXI_HAS_ATTR(preferred_name)
# define EXI_PREFER_NAME(alias) __attribute__((__preferred_name__(alias)))
#else
# define EXI_PREFER_NAME(alias)
#endif

#if EXI_HAS_ATTR(preferred_type)
#define EXI_PREFER_TYPE(T) __attribute__((preferred_type(T)))
#else
#define EXI_PREFER_TYPE(T)
#endif

#ifdef __GNUC__
# define EXI_RETURNS_NOALIAS __attribute__((__malloc__))
#elif defined(_MSC_VER)
# define EXI_RETURNS_NOALIAS __declspec(restrict)
#else
# define EXI_RETURNS_NOALIAS
#endif

#if EXI_HAS_ATTR(used)
# define EXI_USED __attribute__((__used__))
#else
# define EXI_USED
#endif

#if EXI_HAS_ATTR(unused)
# define EXI_UNUSED __attribute__((__unused__))
#else
# define EXI_UNUSED
#endif

// Prior to clang 3.2, clang did not accept any spelling of
// __has_attribute(const), so assume it is supported.
#if defined(__clang__) || defined(__GNUC__)
# define EXI_READNONE __attribute__((__const__))
#else
# define EXI_READNONE
#endif

#if EXI_HAS_ATTR(pure) || defined(__GNUC__)
# define EXI_READONLY __attribute__((__pure__))
#else
# define EXI_READONLY
#endif

#if EXI_HAS_ATTR(returns_nonnull)
# define EXI_RETURNS_NONNULL __attribute__((returns_nonnull))
#elif defined(_MSC_VER)
# define EXI_RETURNS_NONNULL _Ret_notnull_
#else
# define EXI_RETURNS_NONNULL
#endif

/// Mark debug helper function definitions like dump() that should not be
/// stripped from debug builds.
/// Note that you should also surround dump() functions with
/// `#if !defined(NDEBUG) || EXI_ENABLE_DUMP` so they do always
/// get stripped in release builds.
// TODO: Move this to a private config.h as it's not usable in public headers.
#if !defined(NDEBUG) || defined(EXI_ENABLE_DUMP)
# define EXI_DUMP_METHOD EXI_NO_INLINE EXI_USED
#else
# define EXI_DUMP_METHOD EXI_NO_INLINE
#endif

#if EXI_HAS_ATTR(uninitialized)
# define EXI_UNINITIALIZED __attribute__((uninitialized))
#else
# define EXI_UNINITIALIZED
#endif

#if EXI_HAS_CPPATTR(clang::musttail)
# define EXI_HAS_MUSTTAIL 1
# define EXI_MUSTTAIL [[clang::musttail]]
#elif EXI_HAS_ATTR(musttail)
# define EXI_HAS_MUSTTAIL 1
# define EXI_MUSTTAIL __attribute__((musttail))
#else
# define EXI_HAS_MUSTTAIL 0
# define EXI_MUSTTAIL 
#endif

/// Forces and verifies tail calls (even in debug) if supported.
#define tail_return EXI_MUSTTAIL return

//////////////////////////////////////////////////////////////////////////
// Builtins

#ifdef __GNUC__
# define EXI_FUNCTION __PRETTY_FUNCTION__
#elif EXI_MSVC
# define EXI_FUNCTION __FUNCSIG__
#else
# define EXI_FUNCTION __func__
#endif // EXI_FUNCTION

#if defined(__GNUC__)
# define EXI_PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)
#else
# define EXI_PREFETCH(addr, rw, locality) (void(0))
#endif

#if EXI_HAS_BUILTIN(__builtin_expect)
# define EXI_EXPECT(v, expr) \
 (__builtin_expect(static_cast<bool>(expr), v))
#else
# define EXI_EXPECT(v, expr) (static_cast<bool>(expr))
#endif

#define EXI_LIKELY(...)  EXI_EXPECT(1, (__VA_ARGS__))
#define EXI_UNLIKELY(...) EXI_EXPECT(0, (__VA_ARGS__))

#if defined(EXI_UNREACHABLE)
// Use the provided definition.
#elif EXI_HAS_BUILTIN(__builtin_unreachable)
# define EXI_UNREACHABLE __builtin_unreachable()
#elif EXI_HAS_BUILTIN(__builtin_assume)
# define EXI_UNREACHABLE __builtin_assume(0)
#elif EXI_COMPILER_MSVC
# define EXI_UNREACHABLE __assume(0)
#endif

/// Returns a pointer with an assumed alignment.
#if EXI_HAS_BUILTIN(__builtin_assume_aligned) || defined(__GNUC__)
# define EXI_ASSUME_ALIGNED(p, a) __builtin_assume_aligned(p, a)
#elif defined(EXI_UNREACHABLE)
# define EXI_ASSUME_ALIGNED(p, a) \
           (((uintptr_t(p) % (a)) == 0) ? (p) : (EXI_UNREACHABLE, (p)))
#else
# define EXI_ASSUME_ALIGNED(p, a) (p)
#endif

#if EXI_HAS_BUILTIN(__builtin_trap) || defined(__GNUC__)
# define EXI_TRAP __builtin_trap()
#elif defined(_MSC_VER)
# define EXI_TRAP __debugbreak()
#else
# define EXI_TRAP *(volatile int*)0x11 = 0
#endif

#if EXI_HAS_BUILTIN(__builtin_debugtrap)
# define EXI_DBGTRAP __builtin_debugtrap()
#elif defined(_MSC_VER)
# define EXI_DBGTRAP __debugbreak()
#else
# define EXI_DBGTRAP (void(0))
#endif

//////////////////////////////////////////////////////////////////////////
// Sanitizers

#if EXI_HAS_ATTR(no_sanitize)
#define EXI_NO_SANITIZE(kind) __attribute__((no_sanitize(kind)))
#else
#define EXI_NO_SANITIZE(kind)
#endif

#if EXI_HAS_FEATURE(memory_sanitizer)
# define EXI_MEMORY_SANITIZER_BUILD 1
# include <sanitizer/msan_interface.h>
# define EXI_NO_SANITIZE_MEMORY __attribute__((no_sanitize_memory))
#else
# define EXI_MEMORY_SANITIZER_BUILD 0
# define __msan_allocated_memory(p, size)
# define __msan_unpoison(p, size)
# define EXI_NO_SANITIZE_MEMORY
#endif

#if EXI_HAS_FEATURE(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
# define EXI_ADDRESS_SANITIZER_BUILD 1
# if __has_include(<sanitizer/asan_interface.h>)
#  include <sanitizer/asan_interface.h>
# else
// These declarations exist to support ASan with MSVC. If MSVC eventually ships
// asan_interface.h in their headers, then we can remove this.
#  ifdef __cplusplus
extern "C" {
#  endif
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#  ifdef __cplusplus
} // extern "C"
#  endif
# endif
#else
# define EXI_ADDRESS_SANITIZER_BUILD 0
# define __asan_poison_memory_region(p, size)
# define __asan_unpoison_memory_region(p, size)
#endif

//======================================================================//
// Language Version
//======================================================================//

#if __cpp_concepts >= 201907L
# define EXI_CONCEPTS 1
#endif

#define EXI_CXPR11 constexpr

#if EXI_CPP(14)
# define EXI_CXPR14 constexpr
#else
/// Constexpr in C++14.
# define EXI_CXPR14
#endif

#if EXI_CPP(17)
# define EXI_CXPR17 constexpr
#else
/// Constexpr in C++17.
# define EXI_CXPR17
#endif

#if EXI_CPP(20)
# define EXI_CXPR20 constexpr
#else
/// Constexpr in C++20.
# define EXI_CXPR20
#endif

//======================================================================//
// Miscellaneous
//======================================================================//

#define EXI_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

/// Defines a global constant
#define EXI_CONST inline constexpr

namespace exi {
template <typename T>
consteval bool exi_compile_failure() {
  return false;
}

template <auto V>
consteval bool exi_compile_failure() {
  return false;
}
} // namespace exi

#define COMPILE_FAILURE(ty, ...) static_assert( \
  ::exi::exi_compile_failure<ty>() __VA_OPT__(, "In "#ty": ") __VA_ARGS__);
