//===--- DemangleConfig.hpp -------------------------------------*- C++ -*-===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a variety of feature test macros copied from
// include/llvm/Support/Compiler.h so that LLVMDemangle does not need to take
// a dependency on LLVMSupport.
//
//===----------------------------------------------------------------------===//

#ifndef EXI_DEMANGLE_DEMANGLECONFIG_HPP
#define EXI_DEMANGLE_DEMANGLECONFIG_HPP

// llvm-config.h is required for LLVM_ENABLE_LLVM_EXPORT_ANNOTATIONS
//#include "llvm/Config/llvm-config.h"
#include <core/Common/Features.hpp>

#ifndef DEMANGLE_GNUC_PREREQ
# if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#  define DEMANGLE_GNUC_PREREQ(maj, min, patch)                           \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) + __GNUC_PATCHLEVEL__ >=          \
     ((maj) << 20) + ((min) << 10) + (patch))
# elif defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define DEMANGLE_GNUC_PREREQ(maj, min, patch)                           \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) >= ((maj) << 20) + ((min) << 10))
# else
#  define DEMANGLE_GNUC_PREREQ(maj, min, patch) 0
# endif
#endif

#if EXI_HAS_ATTR(used) || DEMANGLE_GNUC_PREREQ(3, 1, 0)
# define DEMANGLE_ATTRIBUTE_USED __attribute__((__used__))
#else
# define DEMANGLE_ATTRIBUTE_USED
#endif

#if EXI_HAS_BUILTIN(__builtin_unreachable) || DEMANGLE_GNUC_PREREQ(4, 5, 0)
# define DEMANGLE_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
# define DEMANGLE_UNREACHABLE __assume(false)
#else
# define DEMANGLE_UNREACHABLE ((void)0)
#endif

#if EXI_HAS_ATTR(noinline) || DEMANGLE_GNUC_PREREQ(3, 4, 0)
# define DEMANGLE_ATTRIBUTE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define DEMANGLE_ATTRIBUTE_NOINLINE __declspec(noinline)
#else
# define DEMANGLE_ATTRIBUTE_NOINLINE
#endif

#if !defined(NDEBUG)
# define DEMANGLE_DUMP_METHOD DEMANGLE_ATTRIBUTE_NOINLINE DEMANGLE_ATTRIBUTE_USED
#else
# define DEMANGLE_DUMP_METHOD DEMANGLE_ATTRIBUTE_NOINLINE
#endif

#if __cplusplus > 201402L && EXI_HAS_CPPATTR(fallthrough)
# define DEMANGLE_FALLTHROUGH [[fallthrough]]
#elif EXI_HAS_CPPATTR(gnu::fallthrough)
# define DEMANGLE_FALLTHROUGH [[gnu::fallthrough]]
#elif !__cplusplus
// Workaround for llvm.org/PR23435, since clang 3.6 and below emit a spurious
// error when EXI_HAS_CPPATTR is given a scoped attribute in C mode.
# define DEMANGLE_FALLTHROUGH
#elif EXI_HAS_CPPATTR(clang::fallthrough)
# define DEMANGLE_FALLTHROUGH [[clang::fallthrough]]
#else
# define DEMANGLE_FALLTHROUGH
#endif

#ifndef DEMANGLE_ASSERT
# include <core/Support/ErrorHandle.hpp>
# if EXI_ASSERTS
#  define DEMANGLE_ASSERT(__expr, __msg) exi_assert_(ASK_Assert, (__expr), __msg)
# else
#  define DEMANGLE_ASSERT(__expr, __msg) exi_cxpr_assert(__expr)
# endif
#endif

#define DEMANGLE_NAMESPACE_BEGIN namespace exi { namespace itanium_demangle {
#define DEMANGLE_NAMESPACE_END } }

/// DEMANGLE_ABI is the export/visibility macro used to mark symbols declared in
/// llvm/Demangle as exported when built as a shared library.
// clang-format off
// Autoformatting removes indentation, making this harder to read.
#ifndef DEMANGLE_ABI
/// TODO: Implement checks for export annotations!
# define DEMANGLE_ABI
/*
# if defined(LLVM_BUILD_STATIC) || !defined(LLVM_ENABLE_LLVM_EXPORT_ANNOTATIONS)
#  define DEMANGLE_ABI
# else
#  if defined(_WIN32) && !defined(__MINGW32__)
#   if defined(LLVM_EXPORTS)
#    define DEMANGLE_ABI __declspec(dllexport)
#   else
#    define DEMANGLE_ABI __declspec(dllimport)
#   endif
#  else
#   if EXI_HAS_ATTR(visibility)
#    define DEMANGLE_ABI __attribute__((__visibility__("default")))
#   else
#    define DEMANGLE_ABI
#   endif
#  endif
# endif
*/
#endif
// clang-format on

#endif
