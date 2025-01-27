//===- Interception.hpp ---------------------------------------------===//
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
/// This file declares the interface for detour functions.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Patches.hpp>
#include <Fundamental.hpp>

#undef STUB_ATTRS
#undef FUNC_NAME
#undef FUNC_TYPE
#undef FUNC_TAG
#undef FUNC_PTR

#ifdef _MSC_VER
# define STUB_ATTRS __noinline
#elif __GNUC__
# define STUB_ATTRS __attribute__((__noinline__, used))
#else
# define STUB_ATTRS
#endif

#define FUNC(name) _mi_##name
#define FUNC_NAME(name) FUNC(name)
#define FUNC_TYPE(name) name##_t
#define FUNC_PTR(name) name##_ptr

#define TERM(name) _mi_##name##_term
#define TERM_NAME(name) TERM(name)
#define TERM_TYPE(name) name##_term_t
#define TERM_PTR(name) name##_term_ptr

#define STUB(name) _mi_##name##_STUB
#define STUB_PTR(name) name##_STUB_ptr

/// Declares a function.
#define FUNC_DECL(cc, ret, name, ...)                           \
 STUB_ATTRS static ret cc                                       \
  FUNC_NAME(name)(__VA_ARGS__) noexcept
/// Defines a type alias for a function.
#define FUNC_TYPE_DECL(cc, ret, name, ...)                      \
 using FUNC_TYPE(name) = ret(*cc)(__VA_ARGS__) noexcept;
/// Defines a pointer to a function.
#define FUNC_PTR_DECL(name)                                     \
 static constexpr FUNC_TYPE(name)                               \
  FUNC_PTR(name) = &FUNC_NAME(name)

/// Declares all the required fields for a function.
#define DECLARE_FUNC_FIELDS(cc, ret, name, ...)                 \
 FUNC_DECL(cc, ret, name, __VA_ARGS__);                         \
 FUNC_TYPE_DECL(cc, ret, name, __VA_ARGS__)                     \
 FUNC_PTR_DECL(name);

/// Creates a usable declaration for a function.
#define DECLARE_FUNC(ret, name, ...)                            \
 DECLARE_FUNC_FIELDS(,ret, name, __VA_ARGS__)                   \
 FUNC_DECL(,ret, name, __VA_ARGS__)
/// Creates a usable declaration for a term function.
#define DECLARE_TERM(ret, name, ...)                            \
 DECLARE_FUNC_FIELDS(,ret, name##_term, __VA_ARGS__)            \
 FUNC_DECL(,ret, name##_term, __VA_ARGS__)
/// Creates a usable declaration for a Microsoft function.
#define DECLARE_MS_FUNC(ret, name, ...)                         \
 DECLARE_FUNC_FIELDS(__stdcall, ret, name, __VA_ARGS__)         \
 FUNC_DECL(__stdcall, ret, name, __VA_ARGS__)

/// Defines a stub function.
#define DEFINE_STUB(ret, name, ...)                             \
DECLARE_FUNC(ret, name##_STUB, __VA_ARGS__)                     \
{ return ::re::RetTemplate<ret>(); }

namespace re {

STUB_ATTRS static void*
 Identity(volatile void* Ptr) noexcept {
  return Ptr;
}

STUB_ATTRS static uptr
 Identity(volatile uptr Val) noexcept {
  return Val;
}

template <typename Ret>
ALWAYS_INLINE static Ret
 RetTemplate() noexcept {
  volatile uptr A = Identity(1);
  Identity(A);
  return (Ret)(0);
}

} // namespace re
